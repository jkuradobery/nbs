// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.26.0
// 	protoc        v3.19.0
// source: cloud/filestore/config/vfs.proto

package config

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type TVFSConfig struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	// FileSystem identifier.
	FileSystemId *string `protobuf:"bytes,1,opt,name=FileSystemId" json:"FileSystemId,omitempty"`
	// Client identifier.
	ClientId *string `protobuf:"bytes,2,opt,name=ClientId" json:"ClientId,omitempty"`
	// Socket for the endpoint.
	SocketPath *string `protobuf:"bytes,3,opt,name=SocketPath" json:"SocketPath,omitempty"`
	// Mount options.
	MountPath *string `protobuf:"bytes,4,opt,name=MountPath" json:"MountPath,omitempty"`
	Debug     *bool   `protobuf:"varint,6,opt,name=Debug" json:"Debug,omitempty"`
	ReadOnly  *bool   `protobuf:"varint,7,opt,name=ReadOnly" json:"ReadOnly,omitempty"`
	// FileSystem generation.
	MountSeqNumber *uint64 `protobuf:"varint,8,opt,name=MountSeqNumber" json:"MountSeqNumber,omitempty"`
	// Vhost queues count.
	VhostQueuesCount *uint32 `protobuf:"varint,9,opt,name=VhostQueuesCount" json:"VhostQueuesCount,omitempty"`
	// Limits max single request length.
	MaxWritePages *uint32 `protobuf:"varint,10,opt,name=MaxWritePages" json:"MaxWritePages,omitempty"`
	// Keep attempts to acquire lock.
	LockRetryTimeout *uint32 `protobuf:"varint,11,opt,name=LockRetryTimeout" json:"LockRetryTimeout,omitempty"`
	// Limits max requests allowed at a time.
	MaxBackground *uint32 `protobuf:"varint,12,opt,name=MaxBackground" json:"MaxBackground,omitempty"`
}

func (x *TVFSConfig) Reset() {
	*x = TVFSConfig{}
	if protoimpl.UnsafeEnabled {
		mi := &file_cloud_filestore_config_vfs_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *TVFSConfig) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*TVFSConfig) ProtoMessage() {}

func (x *TVFSConfig) ProtoReflect() protoreflect.Message {
	mi := &file_cloud_filestore_config_vfs_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use TVFSConfig.ProtoReflect.Descriptor instead.
func (*TVFSConfig) Descriptor() ([]byte, []int) {
	return file_cloud_filestore_config_vfs_proto_rawDescGZIP(), []int{0}
}

func (x *TVFSConfig) GetFileSystemId() string {
	if x != nil && x.FileSystemId != nil {
		return *x.FileSystemId
	}
	return ""
}

func (x *TVFSConfig) GetClientId() string {
	if x != nil && x.ClientId != nil {
		return *x.ClientId
	}
	return ""
}

func (x *TVFSConfig) GetSocketPath() string {
	if x != nil && x.SocketPath != nil {
		return *x.SocketPath
	}
	return ""
}

func (x *TVFSConfig) GetMountPath() string {
	if x != nil && x.MountPath != nil {
		return *x.MountPath
	}
	return ""
}

func (x *TVFSConfig) GetDebug() bool {
	if x != nil && x.Debug != nil {
		return *x.Debug
	}
	return false
}

func (x *TVFSConfig) GetReadOnly() bool {
	if x != nil && x.ReadOnly != nil {
		return *x.ReadOnly
	}
	return false
}

func (x *TVFSConfig) GetMountSeqNumber() uint64 {
	if x != nil && x.MountSeqNumber != nil {
		return *x.MountSeqNumber
	}
	return 0
}

func (x *TVFSConfig) GetVhostQueuesCount() uint32 {
	if x != nil && x.VhostQueuesCount != nil {
		return *x.VhostQueuesCount
	}
	return 0
}

func (x *TVFSConfig) GetMaxWritePages() uint32 {
	if x != nil && x.MaxWritePages != nil {
		return *x.MaxWritePages
	}
	return 0
}

func (x *TVFSConfig) GetLockRetryTimeout() uint32 {
	if x != nil && x.LockRetryTimeout != nil {
		return *x.LockRetryTimeout
	}
	return 0
}

func (x *TVFSConfig) GetMaxBackground() uint32 {
	if x != nil && x.MaxBackground != nil {
		return *x.MaxBackground
	}
	return 0
}

var File_cloud_filestore_config_vfs_proto protoreflect.FileDescriptor

var file_cloud_filestore_config_vfs_proto_rawDesc = []byte{
	0x0a, 0x20, 0x63, 0x6c, 0x6f, 0x75, 0x64, 0x2f, 0x66, 0x69, 0x6c, 0x65, 0x73, 0x74, 0x6f, 0x72,
	0x65, 0x2f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2f, 0x76, 0x66, 0x73, 0x2e, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x12, 0x18, 0x4e, 0x43, 0x6c, 0x6f, 0x75, 0x64, 0x2e, 0x4e, 0x46, 0x69, 0x6c, 0x65,
	0x53, 0x74, 0x6f, 0x72, 0x65, 0x2e, 0x4e, 0x50, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x88, 0x03, 0x0a,
	0x0a, 0x54, 0x56, 0x46, 0x53, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x12, 0x22, 0x0a, 0x0c, 0x46,
	0x69, 0x6c, 0x65, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x49, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x09, 0x52, 0x0c, 0x46, 0x69, 0x6c, 0x65, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x49, 0x64, 0x12,
	0x1a, 0x0a, 0x08, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x49, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28,
	0x09, 0x52, 0x08, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x49, 0x64, 0x12, 0x1e, 0x0a, 0x0a, 0x53,
	0x6f, 0x63, 0x6b, 0x65, 0x74, 0x50, 0x61, 0x74, 0x68, 0x18, 0x03, 0x20, 0x01, 0x28, 0x09, 0x52,
	0x0a, 0x53, 0x6f, 0x63, 0x6b, 0x65, 0x74, 0x50, 0x61, 0x74, 0x68, 0x12, 0x1c, 0x0a, 0x09, 0x4d,
	0x6f, 0x75, 0x6e, 0x74, 0x50, 0x61, 0x74, 0x68, 0x18, 0x04, 0x20, 0x01, 0x28, 0x09, 0x52, 0x09,
	0x4d, 0x6f, 0x75, 0x6e, 0x74, 0x50, 0x61, 0x74, 0x68, 0x12, 0x14, 0x0a, 0x05, 0x44, 0x65, 0x62,
	0x75, 0x67, 0x18, 0x06, 0x20, 0x01, 0x28, 0x08, 0x52, 0x05, 0x44, 0x65, 0x62, 0x75, 0x67, 0x12,
	0x1a, 0x0a, 0x08, 0x52, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0x18, 0x07, 0x20, 0x01, 0x28,
	0x08, 0x52, 0x08, 0x52, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0x12, 0x26, 0x0a, 0x0e, 0x4d,
	0x6f, 0x75, 0x6e, 0x74, 0x53, 0x65, 0x71, 0x4e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0x18, 0x08, 0x20,
	0x01, 0x28, 0x04, 0x52, 0x0e, 0x4d, 0x6f, 0x75, 0x6e, 0x74, 0x53, 0x65, 0x71, 0x4e, 0x75, 0x6d,
	0x62, 0x65, 0x72, 0x12, 0x2a, 0x0a, 0x10, 0x56, 0x68, 0x6f, 0x73, 0x74, 0x51, 0x75, 0x65, 0x75,
	0x65, 0x73, 0x43, 0x6f, 0x75, 0x6e, 0x74, 0x18, 0x09, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x10, 0x56,
	0x68, 0x6f, 0x73, 0x74, 0x51, 0x75, 0x65, 0x75, 0x65, 0x73, 0x43, 0x6f, 0x75, 0x6e, 0x74, 0x12,
	0x24, 0x0a, 0x0d, 0x4d, 0x61, 0x78, 0x57, 0x72, 0x69, 0x74, 0x65, 0x50, 0x61, 0x67, 0x65, 0x73,
	0x18, 0x0a, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0d, 0x4d, 0x61, 0x78, 0x57, 0x72, 0x69, 0x74, 0x65,
	0x50, 0x61, 0x67, 0x65, 0x73, 0x12, 0x2a, 0x0a, 0x10, 0x4c, 0x6f, 0x63, 0x6b, 0x52, 0x65, 0x74,
	0x72, 0x79, 0x54, 0x69, 0x6d, 0x65, 0x6f, 0x75, 0x74, 0x18, 0x0b, 0x20, 0x01, 0x28, 0x0d, 0x52,
	0x10, 0x4c, 0x6f, 0x63, 0x6b, 0x52, 0x65, 0x74, 0x72, 0x79, 0x54, 0x69, 0x6d, 0x65, 0x6f, 0x75,
	0x74, 0x12, 0x24, 0x0a, 0x0d, 0x4d, 0x61, 0x78, 0x42, 0x61, 0x63, 0x6b, 0x67, 0x72, 0x6f, 0x75,
	0x6e, 0x64, 0x18, 0x0c, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0d, 0x4d, 0x61, 0x78, 0x42, 0x61, 0x63,
	0x6b, 0x67, 0x72, 0x6f, 0x75, 0x6e, 0x64, 0x42, 0x29, 0x5a, 0x27, 0x61, 0x2e, 0x79, 0x61, 0x6e,
	0x64, 0x65, 0x78, 0x2d, 0x74, 0x65, 0x61, 0x6d, 0x2e, 0x72, 0x75, 0x2f, 0x63, 0x6c, 0x6f, 0x75,
	0x64, 0x2f, 0x66, 0x69, 0x6c, 0x65, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x2f, 0x63, 0x6f, 0x6e, 0x66,
	0x69, 0x67,
}

var (
	file_cloud_filestore_config_vfs_proto_rawDescOnce sync.Once
	file_cloud_filestore_config_vfs_proto_rawDescData = file_cloud_filestore_config_vfs_proto_rawDesc
)

func file_cloud_filestore_config_vfs_proto_rawDescGZIP() []byte {
	file_cloud_filestore_config_vfs_proto_rawDescOnce.Do(func() {
		file_cloud_filestore_config_vfs_proto_rawDescData = protoimpl.X.CompressGZIP(file_cloud_filestore_config_vfs_proto_rawDescData)
	})
	return file_cloud_filestore_config_vfs_proto_rawDescData
}

var file_cloud_filestore_config_vfs_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_cloud_filestore_config_vfs_proto_goTypes = []interface{}{
	(*TVFSConfig)(nil), // 0: NCloud.NFileStore.NProto.TVFSConfig
}
var file_cloud_filestore_config_vfs_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_cloud_filestore_config_vfs_proto_init() }
func file_cloud_filestore_config_vfs_proto_init() {
	if File_cloud_filestore_config_vfs_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_cloud_filestore_config_vfs_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*TVFSConfig); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_cloud_filestore_config_vfs_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_cloud_filestore_config_vfs_proto_goTypes,
		DependencyIndexes: file_cloud_filestore_config_vfs_proto_depIdxs,
		MessageInfos:      file_cloud_filestore_config_vfs_proto_msgTypes,
	}.Build()
	File_cloud_filestore_config_vfs_proto = out.File
	file_cloud_filestore_config_vfs_proto_rawDesc = nil
	file_cloud_filestore_config_vfs_proto_goTypes = nil
	file_cloud_filestore_config_vfs_proto_depIdxs = nil
}
