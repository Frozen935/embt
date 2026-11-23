/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @brief Release test providers and PCM synchronization resources.
 * @param[in,out] state Unused cmocka fixture state.
 * @return Zero after cleanup.
 */
#ifndef EMBT_TEST_AUDIO_FAKE_H_
#define EMBT_TEST_AUDIO_FAKE_H_
#include "driver.h"

extern bool fake_hold;
extern int fake_error;
extern unsigned char fake_pcm[262144];
extern size_t fake_length;
extern const struct bt_audio_driver fake_driver;
extern const struct bt_audio_driver other_driver;
void fake_pump(void);
int audio_setup(void **state);
int audio_teardown(void **state);
#endif
