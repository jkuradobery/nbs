#!/usr/bin/env bash

d="/root"
scripts="${d}/runner"
suffix="$(date +%y-%m-%d)"
testname="coreutils"

function run_test () {
    local cluster="$1"

    results_dir="/var/www/build/results/${testname}/${cluster}/nfs/${suffix}"
    mkdir -p "$results_dir"

    $d/yc-nfs-ci-build-arcadia-test --cluster "${cluster}" --zone-id eu-north1-b --test-case nfs-coreutils --platform-ids standard-v2 \
        --profile-name "${cluster}-tests" --debug --verbose --cluster-config-path $d/fio_dep/cluster-configs/ --ssh-key-path /root/.ssh/test-ssh-key \
        --no-generate-ycp-config --ycp-requests-template-path $d/fio_dep/ycp-request-templates 2>> "${results_dir}/${testname}".err >> "${results_dir}/${testname}".out

    $scripts/generate_report_coreutils.py junit_report.xml "${cluster}/nfs" "${suffix}" "/var/www/build/results/${testname}.xml" \
        tests/cp/link-symlink.sh \
        tests/ls/stat-free-color.sh \
        tests/mv/mv-special-1.sh \
        tests/tail/inotify-dir-recreate.sh \
        tests/tail/inotify-rotate-resources.sh \
        tests/tail-2/inotify-dir-recreate.sh \
        tests/tail-2/inotify-rotate-resources.sh

    mv junit_report.xml "$results_dir/"
}

run_test nemax
