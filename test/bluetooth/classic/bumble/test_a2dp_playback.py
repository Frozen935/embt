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

"""Bumble A2DP source -> EmBT shared SBC player -> real Pulse sink monitor."""
import asyncio
import io
import json
import os
import re
import subprocess
import time
import uuid
from pathlib import Path

import numpy as np
import pytest
from bumble.a2dp import SbcParser, SbcMediaCodecInformation, A2DP_SBC_CODEC_TYPE
from bumble.avdtp import Protocol, MediaCodecCapabilities, AVDTP_AUDIO_MEDIA_TYPE
from bumble.core import BT_BR_EDR_TRANSPORT
from bumble.hci import Address, HCI_REMOTE_USER_TERMINATED_CONNECTION_ERROR
from bumble.device import Device
from bumble.pairing import PairingConfig, PairingDelegate
from bumble.rtp import MediaPacket
from bumble.transport import open_transport

from embt_harness import DeviceAdapter, Shell

ROOT = Path(__file__).resolve().parents[4]


@pytest.fixture
def pulse_sink(tmp_path, monkeypatch):
    """Own one null sink; never change the user's default audio routing."""
    name = 'embt_test_' + uuid.uuid4().hex
    module = subprocess.check_output([
        'pactl', 'load-module', 'module-null-sink', 'sink_name=' + name,
        'rate=48000', 'channels=2',
    ], text=True).strip()
    monkeypatch.setenv('EMBT_AUDIO_DEVICE', name)
    capture_path = tmp_path / 'pulse-capture.s16le'
    capture = capture_path.open('wb')
    errors = (tmp_path / 'parec.log').open('wb')
    process = subprocess.Popen([
        'parec', '--raw', '--device=' + name + '.monitor', '--format=s16le',
        '--rate=48000', '--channels=2', '--latency-msec=10',
    ], stdout=capture, stderr=errors)
    try:
        time.sleep(0.3)
        assert process.poll() is None
        yield name, capture_path
    finally:
        process.terminate()
        process.wait(timeout=5)
        capture.close()
        errors.close()
        subprocess.run(['pactl', 'unload-module', module], check=True)


def binary(request):
    """Use the selected production btshell executable, not an alternate test sink."""
    override = request.config.getoption('--embt-build')
    return Path(override) if override else ROOT / 'build/audio-playback/bin/btshell'


def test_pulse_lifecycle(pulse_sink, request, tmp_path):
    """Run actual C provider tests against the fixture's PulseAudio sink."""
    executable = binary(request).parents[1] / 'test/audio/test_audio_pulse'
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=40)
    (tmp_path / 'pulse-lifecycle.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stdout + result.stderr
    assert '[  PASSED  ] 4 test(s).' in result.stdout + result.stderr


async def command(shell, text):
    """Keep the Bumble event loop running while waiting for a real shell command."""
    return await asyncio.to_thread(shell.exec_command, text)


async def status(shell, predicate, timeout=12):
    """Poll actual decoder/backend counters with a finite timeout."""
    deadline = time.monotonic() + timeout
    result = None
    while time.monotonic() < deadline:
        lines = await command(shell, 'a2dp audio_status')
        for line in lines:
            match = re.search(r'PCM state (\d+), decoded (\d+), submitted (\d+), '
                              r'consumed (\d+), rejected (\d+), error (-?\d+)', line)
            if match:
                result = dict(zip(('state', 'decoded', 'submitted', 'consumed', 'rejected', 'error'),
                                  map(int, match.groups())))
                assert result['state'] != 6, result
                if predicate(result):
                    return result
        await asyncio.sleep(0.02)
    raise AssertionError(f'PCM state/counters did not converge: {result}')


def encode_tones(rate, mono):
    """Encode independently with FFmpeg and preserve an independent PCM reference."""
    samples = np.arange(int(rate * 0.64), dtype=np.float64)
    envelope = 0.3 + 0.7 * np.sin(np.pi * samples / len(samples)) ** 2
    left = np.sin(samples * (2 * np.pi * 997 / rate)) * 10000 * envelope
    right = np.sin(samples * (2 * np.pi * 1499 / rate)) * 7000 * envelope
    pcm = left[:, None] if mono else np.column_stack((left, right))
    encoded = subprocess.run([
        'ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 's16le', '-ar', str(rate),
        '-ac', '1' if mono else '2', '-i', 'pipe:0', '-c:a', 'sbc',
        '-b:a', '128k' if mono else '224k', '-f', 'sbc', 'pipe:1',
    ], input=pcm.astype('<i2').tobytes(), capture_output=True, check=True).stdout
    reference = subprocess.run([
        'ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 'sbc', '-i', 'pipe:0',
        '-ar', '48000', '-ac', '2', '-f', 's16le', 'pipe:1',
    ], input=encoded, capture_output=True, check=True).stdout
    return encoded, reference


def codec_configuration(frame):
    """Configure exactly the codec parameters in the generated stream."""
    info = SbcMediaCodecInformation
    header = frame[1]
    return MediaCodecCapabilities(AVDTP_AUDIO_MEDIA_TYPE, A2DP_SBC_CODEC_TYPE,
        info(info.SamplingFrequency(1 << (3 - (header >> 6))),
             info.ChannelMode(1 << (3 - ((header >> 2) & 3))),
             info.BlockLength(1 << (3 - ((header >> 4) & 3))),
             info.Subbands(1 if header & 1 else 2),
             info.AllocationMethod(2 if header & 2 else 1), 18, frame[2]))


def correlate_capture(capture, reference, mono):
    """Align independent decoder output and require waveform and channel agreement."""
    captured = np.frombuffer(capture[:len(capture) // 4 * 4], '<i2').reshape(-1, 2).astype(float)
    expected = np.frombuffer(reference, '<i2').reshape(-1, 2).astype(float)
    assert len(captured) >= len(expected), (len(captured), len(expected))
    n = 1 << (len(captured) + len(expected) - 1).bit_length()
    correlation = sum(np.fft.irfft(np.fft.rfft(captured[:, channel], n) *
                                  np.conj(np.fft.rfft(expected[:, channel], n)), n)
                      for channel in (0, 1))
    lag = int(np.argmax(correlation[:len(captured) - len(expected) + 1]))
    aligned = captured[lag:lag + len(expected)]
    coefficients = [float(np.corrcoef(aligned[:, channel], expected[:, channel])[0, 1])
                    for channel in (0, 1)]
    assert min(coefficients) > 0.97, (coefficients, lag)
    peaks = []
    for channel in (0, 1):
        spectrum = abs(np.fft.rfft(aligned[:, channel]))
        peak = float(np.fft.rfftfreq(len(aligned), 1 / 48000)[np.argmax(spectrum)])
        expected_hz = 997 if channel == 0 or mono else 1499
        assert abs(peak - expected_hz) < 8, (channel, peak, expected_hz)
        peaks.append(peak)
    return {'alignment_frames': lag, 'correlation': coefficients, 'peak_hz': peaks,
            'captured_frames': len(captured), 'reference_frames': len(expected)}


@pytest.mark.asyncio
@pytest.mark.parametrize('rate,mono,fragmented', [(48000, False, False),
                                               (48000, True, True),
                                               (44100, False, False)])
async def test_bumble_a2dp_pcm(controller_pair, pulse_sink, request, tmp_path,
                             rate, mono, fragmented):
    """Negotiate real AVDTP and verify every decoded frame through native Pulse output."""
    dut = DeviceAdapter([str(binary(request)), '--tcp', '127.0.0.1', str(controller_pair[0])],
                        tmp_path / 'embt.log')
    shell = Shell(dut)
    try:
        await asyncio.to_thread(dut.readlines_until, 'Bluetooth is disabled')
        await asyncio.to_thread(dut.wait_for_prompt)
        lines = await command(shell, 'bt init')
        if not any('Bluetooth initialized' in line for line in lines):
            lines += await asyncio.to_thread(dut.readlines_until, 'Bluetooth initialized')
        identity = re.search(r'Identity: *[PR]:(([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})',
                             '\n'.join(lines))
        assert identity, lines
        await command(shell, 'br pscan on')
        await command(shell, 'a2dp register_cb')
        await command(shell, 'a2dp register_ep sink sbc')
        encoded, reference = encode_tones(rate, mono)
        (tmp_path / 'source.sbc').write_bytes(encoded)
        (tmp_path / 'reference.s16le').write_bytes(reference)
        source = io.BytesIO(encoded)

        async def read(length):
            return source.read(length)

        frames = [frame async for frame in SbcParser(read).frames]
        assert len(frames) > 100
        async with await open_transport(
                f'tcp-client:127.0.0.1:{controller_pair[1]}') as transport:
            device = Device.with_hci('EmBT audio peer', Address('F0:F1:F2:F3:F4:F5'),
                                     transport.source, transport.sink)
            device.classic_enabled = True
            device.le_enabled = False
            device.pairing_config_factory = lambda connection: PairingConfig(
                sc=True, mitm=False, bonding=False,
                delegate=PairingDelegate(PairingDelegate.IoCapability.NO_OUTPUT_NO_INPUT))
            await device.power_on()
            connection = await device.connect(identity.group(1), transport=BT_BR_EDR_TRANSPORT)
            await device.authenticate(connection)
            await device.encrypt(connection)
            protocol = await Protocol.connect(connection)
            endpoints = list(await protocol.discover_remote_endpoints())
            endpoint = next(ep for ep in endpoints if ep.tsep == 1)
            local = protocol.add_source(codec_configuration(frames[0].payload), None)
            stream = await protocol.create_stream(local, endpoint)
            await stream.start()
            await status(shell, lambda s: s['state'] == 2)
            sequence = 65534
            timestamp = 0
            started = time.monotonic()

            def send(payload):
                nonlocal sequence
                stream.send_media_packet(MediaPacket(2, 0, 0, 0, sequence, timestamp,
                                                       1, [], 96, payload))
                sequence = (sequence + 1) & 0xffff

            send(b'\x01')
            send(b'\x00' + frames[0].payload)
            send(b'\x11' + frames[0].payload)
            corrupt = bytearray(frames[0].payload)
            corrupt[3] ^= 0xff
            send(b'\x01' + corrupt)
            if fragmented:
                frame = frames.pop(0)
                half = len(frame.payload) // 2
                send(b'\xc0' + frame.payload[:half])
                sequence = (sequence + 1) & 0xffff
                send(b'\xa0' + frame.payload[half:])
                send(b'\xc0' + frame.payload[:half])
                send(b'\xa0' + frame.payload[half:])
                timestamp += frame.sample_count
                await asyncio.sleep(max(0, started + timestamp / rate - time.monotonic()))
            for i in range(0, len(frames), 3):
                group = frames[i:i + 3]
                send(bytes([len(group)]) + b''.join(frame.payload for frame in group))
                count = sum(frame.sample_count for frame in group)
                timestamp += count
                await asyncio.sleep(max(0, started + timestamp / rate - time.monotonic()))
            expected_frames = len(reference) // 4
            decoded_count = len(frames) + int(fragmented)
            counters = await status(shell, lambda s: s['decoded'] == decoded_count and
                                    s['consumed'] == decoded_count * 128)
            assert counters['rejected'] == 4 + int(fragmented), counters
            assert counters['submitted'] == decoded_count * 128, counters
            await command(shell, 'a2dp audio_stop drain')
            await status(shell, lambda s: s['state'] == 4)
            await asyncio.sleep(0.2)
            capture = pulse_sink[1].read_bytes()
            analysis = correlate_capture(capture, reference, mono)
            (tmp_path / 'result.json').write_text(json.dumps({
                'rate': rate, 'mono': mono, 'fragmented': fragmented,
                'pcm': counters, 'waveform': analysis,
                'expected_reference_frames': expected_frames,
            }, indent=2))
            await stream.stop()
            await stream.start()
            await status(shell, lambda s: s['state'] == 2)
            for i in range(0, 12, 3):
                group = frames[i:i + 3]
                send(bytes([len(group)]) + b''.join(frame.payload for frame in group))
                await asyncio.sleep(sum(frame.sample_count for frame in group) / rate)
            resumed = await status(shell, lambda s: s['decoded'] == decoded_count + 12 and
                                   s['consumed'] == (decoded_count + 12) * 128)
            assert resumed['rejected'] == counters['rejected'], resumed
            await command(shell, 'a2dp audio_stop drain')
            await status(shell, lambda s: s['state'] == 4)
            await stream.close()
            await device.disconnect(connection, HCI_REMOTE_USER_TERMINATED_CONNECTION_ERROR)
    finally:
        code = await asyncio.to_thread(dut.close)
        assert code == 0, code
