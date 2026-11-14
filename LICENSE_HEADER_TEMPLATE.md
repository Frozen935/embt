<!--
/****************************************************************************
 *  Copyright (C) 2026 <COPYRIGHT HOLDER>
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
-->

# Apache-2.0 License Header Template

This is the canonical file-header template for new EmBT files. Replace
`<YEAR>` and `<COPYRIGHT HOLDER>` with the applicable year and copyright
attribution. The template does not hard-code an organization.

## C, C++, and header files

Use the complete banner below. A short SPDX-only header is not a substitute
for this project format.

~~~c
/****************************************************************************
 *  Copyright (C) <YEAR> <COPYRIGHT HOLDER>
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
~~~

## Markdown files

Wrap the same notice in an HTML comment:

~~~markdown
<!--
/****************************************************************************
 *  Copyright (C) <YEAR> <COPYRIGHT HOLDER>
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
-->
~~~

## Other file formats

For CMake, Kconfig, Python, and shell files, use the native comment syntax
with the same copyright statement and complete Apache-2.0 notice. Preserve
a script's shebang as its first line.

## Imported files

Preserve existing upstream copyright and license notices. Pure relocation
or cherry-picking does not replace another contributor's copyright.
Use the applicable copyright attribution for actual new or modified content.

Use the actual attribution and year rather than copying example values.
