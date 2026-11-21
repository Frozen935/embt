# /****************************************************************************
#  *  Copyright (C) 2026 EmBT contributors
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  ***************************************************************************/
#

"""BR/EDR test-controller pairing with real host decisions and key checks.

This models Controller-owned authentication events for Host integration tests.
It does not implement radio encryption or replace Host security decisions.
"""
import asyncio
import secrets
from dataclasses import dataclass, field
from bumble import hci


@dataclass
class Pairing:
    controllers: tuple
    initiators: set = field(default_factory=set)
    timeout: asyncio.TimerHandle | None = None
    keys: dict = field(default_factory=dict)
    io: dict = field(default_factory=dict)
    pins: dict = field(default_factory=dict)
    decisions: dict = field(default_factory=dict)
    stage: str = 'keys'
    method: str = ''
    number: int = 123456
    secure: bool = False

    def peer(self, controller):
        return self.controllers[1] if controller is self.controllers[0] else self.controllers[0]


class ClassicPairing:
    def __init__(self, link):
        self.link = link
        self.sessions = {}

    def find_peer(self, controller, address):
        return next((c for c in self.link.controllers
                     if c is not controller and c.public_address == address), None)

    def key(self, controller, peer):
        return frozenset((controller, peer))

    def get(self, controller, address):
        peer = self.find_peer(controller, address)
        return self.sessions.get(self.key(controller, peer)) if peer else None

    def emit(self, controller, packet):
        asyncio.get_running_loop().call_soon(controller.send_hci_packet, packet)

    def start(self, controller, connection):
        peer = self.find_peer(controller, connection.peer_address)
        if not peer or not peer._find_classic_connection(controller.public_address):
            return hci.HCI_UNKNOWN_CONNECTION_IDENTIFIER_ERROR
        key = self.key(controller, peer)
        session = self.sessions.get(key)
        if session:
            if controller in session.initiators:
                return hci.HCI_HOST_BUSY_PAIRING_ERROR
            session.initiators.add(controller)
            return hci.HCI_SUCCESS
        session = Pairing((controller, peer), {controller})
        self.sessions[key] = session
        session.timeout = asyncio.get_running_loop().call_later(30, self.finish, session, False)
        self.emit(controller, hci.HCI_Link_Key_Request_Event(bd_addr=peer.public_address))
        return hci.HCI_SUCCESS

    def key_reply(self, controller, address, value):
        session = self.get(controller, address)
        if not session or session.stage != 'keys':
            return
        session.keys[controller] = value
        peer = session.peer(controller)
        if peer not in session.keys:
            self.emit(peer, hci.HCI_Link_Key_Request_Event(bd_addr=controller.public_address))
            return
        values = list(session.keys.values())
        if all(value is not None for value in values):
            if values[0] == values[1]:
                self.finish(session, True, reused=True)
            else:
                self.finish(session, False)
            return
        if session.keys[session.controllers[0]] is not None:
            self.finish(session, False, status_override=hci.HCI_PIN_OR_KEY_MISSING_ERROR)
            return
        session.secure = all(getattr(c, 'secure_connections_host_support', 0)
                             for c in session.controllers)
        if all(getattr(c, 'simple_pairing_mode', 0) for c in session.controllers):
            session.stage = 'io'
            initiator, responder = session.controllers
            self.emit(initiator, hci.HCI_IO_Capability_Request_Event(bd_addr=responder.public_address))
        else:
            session.stage = 'pin'
            for c in session.controllers:
                self.emit(c, hci.HCI_PIN_Code_Request_Event(bd_addr=session.peer(c).public_address))

    def pin_reply(self, controller, address, value):
        session = self.get(controller, address)
        if not session or session.stage != 'pin':
            return
        if value is None:
            self.finish(session, False, legacy=True)
            return
        session.pins[controller] = value
        if len(session.pins) == 2:
            pins = list(session.pins.values())
            self.finish(session, pins[0] == pins[1], legacy=True)

    def io_reply(self, controller, command):
        session = self.get(controller, command.bd_addr)
        if not session or session.stage != 'io':
            return
        session.io[controller] = (int(command.io_capability), int(command.authentication_requirements))
        peer = session.peer(controller)
        self.emit(peer, hci.HCI_IO_Capability_Response_Event(
            bd_addr=controller.public_address, io_capability=int(command.io_capability),
            oob_data_present=0, authentication_requirements=int(command.authentication_requirements)))
        if len(session.io) != 2:
            self.emit(peer, hci.HCI_IO_Capability_Request_Event(bd_addr=controller.public_address))
            return
        caps = [session.io[c][0] for c in session.controllers]
        mitm = any(session.io[c][1] & 1 for c in session.controllers)
        if not mitm or 3 in caps:
            session.method = 'just_works'
        elif all(cap in (1, 4) for cap in caps):
            session.method = 'numeric'
        elif any(cap in (2, 4) for cap in caps) and any(cap in (0, 1, 4) for cap in caps):
            session.method = 'passkey'
        elif caps == [2, 2]:
            session.method = 'passkey'
        else:
            session.method = 'just_works'
        session.stage = 'decision'
        if session.method in ('numeric', 'just_works'):
            for c in session.controllers:
                self.emit(c, hci.HCI_User_Confirmation_Request_Event(
                    bd_addr=session.peer(c).public_address, numeric_value=session.number))
        else:
            for c in session.controllers:
                capability = session.io[c][0]
                peer = session.peer(c)
                # A keyboard displays only when the peer is also a keyboard.
                if capability == 2 or (capability == 4 and session.io[peer][0] in (0, 1)):
                    self.emit(c, hci.HCI_User_Passkey_Request_Event(bd_addr=peer.public_address))
                else:
                    session.decisions[c] = True
                    self.emit(c, hci.HCI_User_Passkey_Notification_Event(
                        bd_addr=peer.public_address, passkey=session.number))

    def decision(self, controller, address, accepted):
        session = self.get(controller, address)
        if not session or session.stage != 'decision':
            return
        if not accepted:
            self.finish(session, False)
            return
        session.decisions[controller] = True
        if len(session.decisions) == 2:
            self.finish(session, True)

    def passkey(self, controller, address, value):
        session = self.get(controller, address)
        if session:
            self.decision(controller, address, value == session.number)

    def reject(self, controller, address):
        session = self.get(controller, address)
        if session:
            self.finish(session, False)

    def finish(self, session, success, legacy=False, reused=False, status_override=None):
        if self.sessions.get(self.key(*session.controllers)) is not session:
            return
        self.sessions.pop(self.key(*session.controllers))
        if session.timeout:
            session.timeout.cancel()
        status = hci.HCI_SUCCESS if success else hci.HCI_AUTHENTICATION_FAILURE_ERROR
        if status_override is not None:
            status = status_override
        authenticated = session.method in ('numeric', 'passkey')
        if legacy:
            key_type = hci.LinkKeyType.COMBINATION_KEY
        elif session.secure:
            key_type = (hci.LinkKeyType.AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P_256 if authenticated
                        else hci.LinkKeyType.UNAUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P_256)
        else:
            key_type = (hci.LinkKeyType.AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P_192 if authenticated
                        else hci.LinkKeyType.UNAUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P_192)
        value = secrets.token_bytes(16) if success and not reused else None
        for c in session.controllers:
            peer = session.peer(c)
            connection = c._find_classic_connection(peer.public_address)
            if not connection:
                continue
            connection.authentication_request = False
            if not legacy and not reused:
                self.emit(c, hci.HCI_Simple_Pairing_Complete_Event(status=status, bd_addr=peer.public_address))
            if success:
                connection.authentication_done = True
                connection.key_refresh_pending = bool(connection.encryption_enabled and not reused)
                connection.key_size = 16
                if not reused:
                    connection.link_key = value
                    connection.link_key_type = key_type
                    c.pairing_keys[peer.public_address] = (value, key_type)
                    self.emit(c, hci.HCI_Link_Key_Notification_Event(
                        bd_addr=peer.public_address, link_key=value, key_type=key_type))
                else:
                    known = c.pairing_keys.get(peer.public_address)
                    if known:
                        connection.link_key, connection.link_key_type = known
            if c in session.initiators:
                self.emit(c, hci.HCI_Authentication_Complete_Event(
                    status=status, connection_handle=connection.handle))

    def disconnected(self, controller, address):
        session = self.get(controller, address)
        if session:
            self.sessions.pop(self.key(*session.controllers), None)
            if session.timeout:
                session.timeout.cancel()
