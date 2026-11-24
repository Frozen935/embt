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


function embt_check_providers()
{
  local embt_root="$(gettop)"
  if [ ! -f "$embt_root/apps/crypto/mbedtls/mbedtls/include/psa/crypto.h" ]; then
    echo "Prepare platform Mbed TLS explicitly before building EmBT" >&2
    exit 1
  fi
  if [ ! -f "$embt_root/apps/testing/cmocka/cmocka/src/cmocka.c" ]; then
    echo "Prepare platform cmocka explicitly before building EmBT tests" >&2
    exit 1
  fi
}

add_lunch_combo "[openvela]-[vela]-[embt]"
add_vendor_prebuild_hook "[openvela]-[vela]-[embt]" embt_check_providers
