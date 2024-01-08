import sys

from .lib import TestRunner, parse_args

from cloud.blockstore.pylibs import common


def main():
    args = parse_args()
    logger = common.create_logger('yc-nbs-ci-validate-checkpoint-test', args)

    try:
        TestRunner(common.ModuleFactories(
            common.make_test_result_processor_stub,
            common.fetch_server_version_stub,
            common.make_config_generator_stub,
            make_ssh_client=common.make_ssh_client,
            make_helpers=common.make_helpers,
            make_sftp_client=common.make_sftp_client,
            make_ssh_channel=common.make_ssh_channel,
        ), args, logger).run_test()
    except Exception as e:
        logger.fatal(f'Test failed: {e}')
        sys.exit(1)
    logger.info('Test finished successfully')


if __name__ == '__main__':
    main()
