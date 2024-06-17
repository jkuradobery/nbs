LIBRARY()

SRCS(
    disk_registry_actor_acquire.cpp
    disk_registry_actor_allocate.cpp
    disk_registry_actor_backup_state.cpp
    disk_registry_actor_change_disk_device.cpp
    disk_registry_actor_checkpoint.cpp
    disk_registry_actor_cleanup.cpp
    disk_registry_actor_cms.cpp
    disk_registry_actor_config.cpp
    disk_registry_actor_create_disk_from_devices.cpp
    disk_registry_actor_describe.cpp
    disk_registry_actor_destroy.cpp
    disk_registry_actor_get_agent_node_id.cpp
    disk_registry_actor_get_dependent_disks.cpp
    disk_registry_actor_initiate_realloc.cpp
    disk_registry_actor_initschema.cpp
    disk_registry_actor_loadstate.cpp
    disk_registry_actor_mark_disk_for_cleanup.cpp
    disk_registry_actor_mark_replacement_device.cpp
    disk_registry_actor_migration.cpp
    disk_registry_actor_monitoring_replace_device.cpp
    disk_registry_actor_monitoring_volume_realloc.cpp
    disk_registry_actor_monitoring.cpp
    disk_registry_actor_notify.cpp
    disk_registry_actor_notify_users.cpp
    disk_registry_actor_placement.cpp
    disk_registry_actor_publish_disk_state.cpp
    disk_registry_actor_writable_state.cpp
    disk_registry_actor_query_agents_info.cpp
    disk_registry_actor_query_available_storage.cpp
    disk_registry_actor_register.cpp
    disk_registry_actor_regular.cpp
    disk_registry_actor_release.cpp
    disk_registry_actor_replace.cpp
    disk_registry_actor_restore_state.cpp
    disk_registry_actor_resume_device.cpp
    disk_registry_actor_secure_erase.cpp
    disk_registry_actor_set_user_id.cpp
    disk_registry_actor_suspend_device.cpp
    disk_registry_actor_switch_agent_to_read_only.cpp
    disk_registry_actor_update_agent_state.cpp
    disk_registry_actor_update_cms_device_state.cpp
    disk_registry_actor_update_cms_host_state.cpp
    disk_registry_actor_update_device_state.cpp
    disk_registry_actor_update_disk_block_size.cpp
    disk_registry_actor_update_disk_replica_count.cpp
    disk_registry_actor_update_params.cpp
    disk_registry_actor_update_placement_group_settings.cpp
    disk_registry_actor_update_stats.cpp
    disk_registry_actor_volume_config.cpp
    disk_registry_actor_waitready.cpp
    disk_registry_actor.cpp
    disk_registry_counters.cpp
    disk_registry_database.cpp
    disk_registry_schema.cpp
    disk_registry_self_counters.cpp
    disk_registry_state.cpp
    disk_registry_state_notification.cpp
    disk_registry.cpp
)

PEERDIR(
    cloud/blockstore/config
    cloud/blockstore/libs/kikimr
    cloud/blockstore/libs/logbroker/iface
    cloud/blockstore/libs/notify
    cloud/blockstore/libs/storage/api
    cloud/blockstore/libs/storage/core
    cloud/blockstore/libs/storage/disk_common
    cloud/blockstore/libs/storage/disk_registry/actors
    cloud/blockstore/libs/storage/disk_registry/model
    cloud/storage/core/libs/common
    cloud/storage/core/libs/diagnostics
    contrib/ydb/library/actors/core
    contrib/ydb/core/base
    contrib/ydb/core/mind
    contrib/ydb/core/mon
    contrib/ydb/core/node_whiteboard
    contrib/ydb/core/scheme
    contrib/ydb/core/tablet
    contrib/ydb/core/tablet_flat
)

END()

RECURSE(
    actors
    model
)

RECURSE_FOR_TESTS(
    ut
    ut_allocation
    ut_cms
    ut_config
    ut_checkpoint
    ut_create
    ut_migration
    ut_mirrored_disk_migration
    ut_notify
    ut_pools
    ut_restore
    ut_session
    ut_suspend
)

# DEVTOOLSSUPPORT-28903
IF (SANITIZER_TYPE != "thread")

RECURSE_FOR_TESTS(
    ut_lifecycle
)

ENDIF()
