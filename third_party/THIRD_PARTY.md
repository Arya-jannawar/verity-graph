# Vendored third-party dependencies

All dependencies are vendored so the project builds and runs fully offline.
No asset is downloaded at runtime.

| Component | Version | File(s) | License | Source |
|---|---|---|---|---|
| nlohmann/json | 3.11.3 | `third_party/nlohmann/json.hpp` | MIT | https://github.com/nlohmann/json/releases/tag/v3.11.3 |
| doctest | 2.4.12 | `third_party/doctest/doctest.h` | MIT | https://github.com/doctest/doctest/releases/tag/v2.4.12 |
| vis-network (standalone UMD bundle) | 9.1.9 | `ui/vendor/vis-network.min.js`, `.map` | Apache-2.0 | https://cdn.jsdelivr.net/npm/vis-network@9.1.9/dist/vis-network.min.js |

Python dependencies (see `ai/requirements.txt`, pinned):
pytest 8.3.4, jsonschema 4.23.0, pypdf 5.1.0, python-docx 1.1.2, reportlab 4.2.5
(+ transitive pins recorded by pip freeze).

MIT license texts are embedded in the respective single-file distributions
(nlohmann/json header and doctest header). vis-network's Apache-2.0 license:
https://github.com/visjs/vis-network/blob/master/LICENSE (also reproduced in
`ui/vendor/LICENSE.vis-network`).
