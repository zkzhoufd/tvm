#!/usr/bin/env bash
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
set -euxo pipefail

python3 -m pylint python/tvm --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint vta/python/vta --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/unittest/test_tvmscript_type.py --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/contrib/test_cmsisnn --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/relay/aot/*.py --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/ci --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/integration/ --rcfile="$(dirname "$0")"/pylintrc

# tests/python/contrib/test_hexagon tests
python3 -m pylint tests/python/contrib/test_hexagon/benchmark_util.py --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/contrib/test_hexagon/conftest.py --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/contrib/test_hexagon/conv2d/test_conv2d_blocked.py --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/contrib/test_hexagon/conv2d/test_conv2d_conv2d.py --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/contrib/test_hexagon/infrastructure.py --rcfile="$(dirname "$0")"/pylintrc
python3 -m pylint tests/python/contrib/test_hexagon/test_2d_physical_buffers.py --rcfile="$(dirname "$0")"/pylintrc
