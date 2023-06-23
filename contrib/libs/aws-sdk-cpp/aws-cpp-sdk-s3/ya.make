# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/aws-sdk-cpp/aws-cpp-sdk-core
    contrib/restricted/aws/aws-c-common
    contrib/restricted/aws/aws-c-event-stream
)

ADDINCL(
    GLOBAL contrib/libs/aws-sdk-cpp/aws-cpp-sdk-s3/include
    contrib/libs/aws-sdk-cpp/aws-cpp-sdk-core/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    -DAWS_CAL_USE_IMPORT_EXPORT
    -DAWS_CHECKSUMS_USE_IMPORT_EXPORT
    -DAWS_COMMON_USE_IMPORT_EXPORT
    -DAWS_EVENT_STREAM_USE_IMPORT_EXPORT
    -DAWS_IO_USE_IMPORT_EXPORT
    -DAWS_SDK_VERSION_MAJOR=1
    -DAWS_SDK_VERSION_MINOR=8
    -DAWS_SDK_VERSION_PATCH=186
    -DAWS_USE_EPOLL
    -DENABLE_CURL_CLIENT
    -DENABLE_OPENSSL_ENCRYPTION
    -DHAS_PATHCONF
    -DHAS_UMASK
    -DS2N_ADX
    -DS2N_BIKE_R3_AVX2
    -DS2N_BIKE_R3_AVX512
    -DS2N_BIKE_R3_PCLMUL
    -DS2N_BIKE_R3_VPCLMUL
    -DS2N_CPUID_AVAILABLE
    -DS2N_FALL_THROUGH_SUPPORTED
    -DS2N_HAVE_EXECINFO
    -DS2N_KYBER512R3_AVX2_BMI2
    -DS2N_SIKE_P434_R3_ASM
    -DS2N___RESTRICT__SUPPORTED
)

SRCS(
    source/S3ARN.cpp
    source/S3Client.cpp
    source/S3Endpoint.cpp
    source/S3ErrorMarshaller.cpp
    source/S3Errors.cpp
    source/model/AbortIncompleteMultipartUpload.cpp
    source/model/AbortMultipartUploadRequest.cpp
    source/model/AbortMultipartUploadResult.cpp
    source/model/AccelerateConfiguration.cpp
    source/model/AccessControlPolicy.cpp
    source/model/AccessControlTranslation.cpp
    source/model/AnalyticsAndOperator.cpp
    source/model/AnalyticsConfiguration.cpp
    source/model/AnalyticsExportDestination.cpp
    source/model/AnalyticsFilter.cpp
    source/model/AnalyticsS3BucketDestination.cpp
    source/model/AnalyticsS3ExportFileFormat.cpp
    source/model/ArchiveStatus.cpp
    source/model/Bucket.cpp
    source/model/BucketAccelerateStatus.cpp
    source/model/BucketCannedACL.cpp
    source/model/BucketLifecycleConfiguration.cpp
    source/model/BucketLocationConstraint.cpp
    source/model/BucketLoggingStatus.cpp
    source/model/BucketLogsPermission.cpp
    source/model/BucketVersioningStatus.cpp
    source/model/CORSConfiguration.cpp
    source/model/CORSRule.cpp
    source/model/CSVInput.cpp
    source/model/CSVOutput.cpp
    source/model/CloudFunctionConfiguration.cpp
    source/model/CommonPrefix.cpp
    source/model/CompleteMultipartUploadRequest.cpp
    source/model/CompleteMultipartUploadResult.cpp
    source/model/CompletedMultipartUpload.cpp
    source/model/CompletedPart.cpp
    source/model/CompressionType.cpp
    source/model/Condition.cpp
    source/model/CopyObjectRequest.cpp
    source/model/CopyObjectResult.cpp
    source/model/CopyObjectResultDetails.cpp
    source/model/CopyPartResult.cpp
    source/model/CreateBucketConfiguration.cpp
    source/model/CreateBucketRequest.cpp
    source/model/CreateBucketResult.cpp
    source/model/CreateMultipartUploadRequest.cpp
    source/model/CreateMultipartUploadResult.cpp
    source/model/DefaultRetention.cpp
    source/model/Delete.cpp
    source/model/DeleteBucketAnalyticsConfigurationRequest.cpp
    source/model/DeleteBucketCorsRequest.cpp
    source/model/DeleteBucketEncryptionRequest.cpp
    source/model/DeleteBucketIntelligentTieringConfigurationRequest.cpp
    source/model/DeleteBucketInventoryConfigurationRequest.cpp
    source/model/DeleteBucketLifecycleRequest.cpp
    source/model/DeleteBucketMetricsConfigurationRequest.cpp
    source/model/DeleteBucketOwnershipControlsRequest.cpp
    source/model/DeleteBucketPolicyRequest.cpp
    source/model/DeleteBucketReplicationRequest.cpp
    source/model/DeleteBucketRequest.cpp
    source/model/DeleteBucketTaggingRequest.cpp
    source/model/DeleteBucketWebsiteRequest.cpp
    source/model/DeleteMarkerEntry.cpp
    source/model/DeleteMarkerReplication.cpp
    source/model/DeleteMarkerReplicationStatus.cpp
    source/model/DeleteObjectRequest.cpp
    source/model/DeleteObjectResult.cpp
    source/model/DeleteObjectTaggingRequest.cpp
    source/model/DeleteObjectTaggingResult.cpp
    source/model/DeleteObjectsRequest.cpp
    source/model/DeleteObjectsResult.cpp
    source/model/DeletePublicAccessBlockRequest.cpp
    source/model/DeletedObject.cpp
    source/model/Destination.cpp
    source/model/EncodingType.cpp
    source/model/Encryption.cpp
    source/model/EncryptionConfiguration.cpp
    source/model/Error.cpp
    source/model/ErrorDocument.cpp
    source/model/Event.cpp
    source/model/ExistingObjectReplication.cpp
    source/model/ExistingObjectReplicationStatus.cpp
    source/model/ExpirationStatus.cpp
    source/model/ExpressionType.cpp
    source/model/FileHeaderInfo.cpp
    source/model/FilterRule.cpp
    source/model/FilterRuleName.cpp
    source/model/GetBucketAccelerateConfigurationRequest.cpp
    source/model/GetBucketAccelerateConfigurationResult.cpp
    source/model/GetBucketAclRequest.cpp
    source/model/GetBucketAclResult.cpp
    source/model/GetBucketAnalyticsConfigurationRequest.cpp
    source/model/GetBucketAnalyticsConfigurationResult.cpp
    source/model/GetBucketCorsRequest.cpp
    source/model/GetBucketCorsResult.cpp
    source/model/GetBucketEncryptionRequest.cpp
    source/model/GetBucketEncryptionResult.cpp
    source/model/GetBucketIntelligentTieringConfigurationRequest.cpp
    source/model/GetBucketIntelligentTieringConfigurationResult.cpp
    source/model/GetBucketInventoryConfigurationRequest.cpp
    source/model/GetBucketInventoryConfigurationResult.cpp
    source/model/GetBucketLifecycleConfigurationRequest.cpp
    source/model/GetBucketLifecycleConfigurationResult.cpp
    source/model/GetBucketLocationRequest.cpp
    source/model/GetBucketLocationResult.cpp
    source/model/GetBucketLoggingRequest.cpp
    source/model/GetBucketLoggingResult.cpp
    source/model/GetBucketMetricsConfigurationRequest.cpp
    source/model/GetBucketMetricsConfigurationResult.cpp
    source/model/GetBucketNotificationConfigurationRequest.cpp
    source/model/GetBucketNotificationConfigurationResult.cpp
    source/model/GetBucketOwnershipControlsRequest.cpp
    source/model/GetBucketOwnershipControlsResult.cpp
    source/model/GetBucketPolicyRequest.cpp
    source/model/GetBucketPolicyResult.cpp
    source/model/GetBucketPolicyStatusRequest.cpp
    source/model/GetBucketPolicyStatusResult.cpp
    source/model/GetBucketReplicationRequest.cpp
    source/model/GetBucketReplicationResult.cpp
    source/model/GetBucketRequestPaymentRequest.cpp
    source/model/GetBucketRequestPaymentResult.cpp
    source/model/GetBucketTaggingRequest.cpp
    source/model/GetBucketTaggingResult.cpp
    source/model/GetBucketVersioningRequest.cpp
    source/model/GetBucketVersioningResult.cpp
    source/model/GetBucketWebsiteRequest.cpp
    source/model/GetBucketWebsiteResult.cpp
    source/model/GetObjectAclRequest.cpp
    source/model/GetObjectAclResult.cpp
    source/model/GetObjectLegalHoldRequest.cpp
    source/model/GetObjectLegalHoldResult.cpp
    source/model/GetObjectLockConfigurationRequest.cpp
    source/model/GetObjectLockConfigurationResult.cpp
    source/model/GetObjectRequest.cpp
    source/model/GetObjectResult.cpp
    source/model/GetObjectRetentionRequest.cpp
    source/model/GetObjectRetentionResult.cpp
    source/model/GetObjectTaggingRequest.cpp
    source/model/GetObjectTaggingResult.cpp
    source/model/GetObjectTorrentRequest.cpp
    source/model/GetObjectTorrentResult.cpp
    source/model/GetPublicAccessBlockRequest.cpp
    source/model/GetPublicAccessBlockResult.cpp
    source/model/GlacierJobParameters.cpp
    source/model/Grant.cpp
    source/model/Grantee.cpp
    source/model/HeadBucketRequest.cpp
    source/model/HeadObjectRequest.cpp
    source/model/HeadObjectResult.cpp
    source/model/IndexDocument.cpp
    source/model/Initiator.cpp
    source/model/InputSerialization.cpp
    source/model/IntelligentTieringAccessTier.cpp
    source/model/IntelligentTieringAndOperator.cpp
    source/model/IntelligentTieringConfiguration.cpp
    source/model/IntelligentTieringFilter.cpp
    source/model/IntelligentTieringStatus.cpp
    source/model/InvalidObjectState.cpp
    source/model/InventoryConfiguration.cpp
    source/model/InventoryDestination.cpp
    source/model/InventoryEncryption.cpp
    source/model/InventoryFilter.cpp
    source/model/InventoryFormat.cpp
    source/model/InventoryFrequency.cpp
    source/model/InventoryIncludedObjectVersions.cpp
    source/model/InventoryOptionalField.cpp
    source/model/InventoryS3BucketDestination.cpp
    source/model/InventorySchedule.cpp
    source/model/JSONInput.cpp
    source/model/JSONOutput.cpp
    source/model/JSONType.cpp
    source/model/LambdaFunctionConfiguration.cpp
    source/model/LifecycleConfiguration.cpp
    source/model/LifecycleExpiration.cpp
    source/model/LifecycleRule.cpp
    source/model/LifecycleRuleAndOperator.cpp
    source/model/LifecycleRuleFilter.cpp
    source/model/ListBucketAnalyticsConfigurationsRequest.cpp
    source/model/ListBucketAnalyticsConfigurationsResult.cpp
    source/model/ListBucketIntelligentTieringConfigurationsRequest.cpp
    source/model/ListBucketIntelligentTieringConfigurationsResult.cpp
    source/model/ListBucketInventoryConfigurationsRequest.cpp
    source/model/ListBucketInventoryConfigurationsResult.cpp
    source/model/ListBucketMetricsConfigurationsRequest.cpp
    source/model/ListBucketMetricsConfigurationsResult.cpp
    source/model/ListBucketsResult.cpp
    source/model/ListMultipartUploadsRequest.cpp
    source/model/ListMultipartUploadsResult.cpp
    source/model/ListObjectVersionsRequest.cpp
    source/model/ListObjectVersionsResult.cpp
    source/model/ListObjectsRequest.cpp
    source/model/ListObjectsResult.cpp
    source/model/ListObjectsV2Request.cpp
    source/model/ListObjectsV2Result.cpp
    source/model/ListPartsRequest.cpp
    source/model/ListPartsResult.cpp
    source/model/LoggingEnabled.cpp
    source/model/MFADelete.cpp
    source/model/MFADeleteStatus.cpp
    source/model/MetadataDirective.cpp
    source/model/MetadataEntry.cpp
    source/model/Metrics.cpp
    source/model/MetricsAndOperator.cpp
    source/model/MetricsConfiguration.cpp
    source/model/MetricsFilter.cpp
    source/model/MetricsStatus.cpp
    source/model/MultipartUpload.cpp
    source/model/NoncurrentVersionExpiration.cpp
    source/model/NoncurrentVersionTransition.cpp
    source/model/NotificationConfiguration.cpp
    source/model/NotificationConfigurationDeprecated.cpp
    source/model/NotificationConfigurationFilter.cpp
    source/model/Object.cpp
    source/model/ObjectCannedACL.cpp
    source/model/ObjectIdentifier.cpp
    source/model/ObjectLockConfiguration.cpp
    source/model/ObjectLockEnabled.cpp
    source/model/ObjectLockLegalHold.cpp
    source/model/ObjectLockLegalHoldStatus.cpp
    source/model/ObjectLockMode.cpp
    source/model/ObjectLockRetention.cpp
    source/model/ObjectLockRetentionMode.cpp
    source/model/ObjectLockRule.cpp
    source/model/ObjectOwnership.cpp
    source/model/ObjectStorageClass.cpp
    source/model/ObjectVersion.cpp
    source/model/ObjectVersionStorageClass.cpp
    source/model/OutputLocation.cpp
    source/model/OutputSerialization.cpp
    source/model/Owner.cpp
    source/model/OwnerOverride.cpp
    source/model/OwnershipControls.cpp
    source/model/OwnershipControlsRule.cpp
    source/model/ParquetInput.cpp
    source/model/Part.cpp
    source/model/Payer.cpp
    source/model/Permission.cpp
    source/model/PolicyStatus.cpp
    source/model/Progress.cpp
    source/model/ProgressEvent.cpp
    source/model/Protocol.cpp
    source/model/PublicAccessBlockConfiguration.cpp
    source/model/PutBucketAccelerateConfigurationRequest.cpp
    source/model/PutBucketAclRequest.cpp
    source/model/PutBucketAnalyticsConfigurationRequest.cpp
    source/model/PutBucketCorsRequest.cpp
    source/model/PutBucketEncryptionRequest.cpp
    source/model/PutBucketIntelligentTieringConfigurationRequest.cpp
    source/model/PutBucketInventoryConfigurationRequest.cpp
    source/model/PutBucketLifecycleConfigurationRequest.cpp
    source/model/PutBucketLoggingRequest.cpp
    source/model/PutBucketMetricsConfigurationRequest.cpp
    source/model/PutBucketNotificationConfigurationRequest.cpp
    source/model/PutBucketOwnershipControlsRequest.cpp
    source/model/PutBucketPolicyRequest.cpp
    source/model/PutBucketReplicationRequest.cpp
    source/model/PutBucketRequestPaymentRequest.cpp
    source/model/PutBucketTaggingRequest.cpp
    source/model/PutBucketVersioningRequest.cpp
    source/model/PutBucketWebsiteRequest.cpp
    source/model/PutObjectAclRequest.cpp
    source/model/PutObjectAclResult.cpp
    source/model/PutObjectLegalHoldRequest.cpp
    source/model/PutObjectLegalHoldResult.cpp
    source/model/PutObjectLockConfigurationRequest.cpp
    source/model/PutObjectLockConfigurationResult.cpp
    source/model/PutObjectRequest.cpp
    source/model/PutObjectResult.cpp
    source/model/PutObjectRetentionRequest.cpp
    source/model/PutObjectRetentionResult.cpp
    source/model/PutObjectTaggingRequest.cpp
    source/model/PutObjectTaggingResult.cpp
    source/model/PutPublicAccessBlockRequest.cpp
    source/model/QueueConfiguration.cpp
    source/model/QueueConfigurationDeprecated.cpp
    source/model/QuoteFields.cpp
    source/model/Redirect.cpp
    source/model/RedirectAllRequestsTo.cpp
    source/model/ReplicaModifications.cpp
    source/model/ReplicaModificationsStatus.cpp
    source/model/ReplicationConfiguration.cpp
    source/model/ReplicationRule.cpp
    source/model/ReplicationRuleAndOperator.cpp
    source/model/ReplicationRuleFilter.cpp
    source/model/ReplicationRuleStatus.cpp
    source/model/ReplicationStatus.cpp
    source/model/ReplicationTime.cpp
    source/model/ReplicationTimeStatus.cpp
    source/model/ReplicationTimeValue.cpp
    source/model/RequestCharged.cpp
    source/model/RequestPayer.cpp
    source/model/RequestPaymentConfiguration.cpp
    source/model/RequestProgress.cpp
    source/model/RestoreObjectRequest.cpp
    source/model/RestoreObjectResult.cpp
    source/model/RestoreRequest.cpp
    source/model/RestoreRequestType.cpp
    source/model/RoutingRule.cpp
    source/model/Rule.cpp
    source/model/S3KeyFilter.cpp
    source/model/S3Location.cpp
    source/model/SSEKMS.cpp
    source/model/SSES3.cpp
    source/model/ScanRange.cpp
    source/model/SelectObjectContentHandler.cpp
    source/model/SelectObjectContentRequest.cpp
    source/model/SelectParameters.cpp
    source/model/ServerSideEncryption.cpp
    source/model/ServerSideEncryptionByDefault.cpp
    source/model/ServerSideEncryptionConfiguration.cpp
    source/model/ServerSideEncryptionRule.cpp
    source/model/SourceSelectionCriteria.cpp
    source/model/SseKmsEncryptedObjects.cpp
    source/model/SseKmsEncryptedObjectsStatus.cpp
    source/model/Stats.cpp
    source/model/StatsEvent.cpp
    source/model/StorageClass.cpp
    source/model/StorageClassAnalysis.cpp
    source/model/StorageClassAnalysisDataExport.cpp
    source/model/StorageClassAnalysisSchemaVersion.cpp
    source/model/Tag.cpp
    source/model/Tagging.cpp
    source/model/TaggingDirective.cpp
    source/model/TargetGrant.cpp
    source/model/Tier.cpp
    source/model/Tiering.cpp
    source/model/TopicConfiguration.cpp
    source/model/TopicConfigurationDeprecated.cpp
    source/model/Transition.cpp
    source/model/TransitionStorageClass.cpp
    source/model/Type.cpp
    source/model/UploadPartCopyRequest.cpp
    source/model/UploadPartCopyResult.cpp
    source/model/UploadPartRequest.cpp
    source/model/UploadPartResult.cpp
    source/model/VersioningConfiguration.cpp
    source/model/WebsiteConfiguration.cpp
    source/model/WriteGetObjectResponseRequest.cpp
)

END()
