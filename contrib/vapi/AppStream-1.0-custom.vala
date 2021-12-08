namespace AppStream {
	[CCode (cheader_filename = "appstream.h", cprefix = "AS_PROVIDED_KIND_", has_type_id = false)]
	public enum ProvidedKind {
		[Version (deprecated = true, replacement = "MEDIATYPE")]
		MIMETYPE
	}

	[CCode (cheader_filename = "appstream.h", cprefix = "AS_COMPONENT_KIND_", has_type_id = false)]
	public enum ComponentKind {
		[Version (deprecated = true, replacement = "INPUT_METHOD")]
		INPUTMETHOD
	}

	[Flags]
	public enum PoolFlags {
		[Version (deprecated = true, replacement = "LOAD_OS_COLLECTION")]
		READ_COLLECTION,
		[Version (deprecated = true, replacement = "LOAD_OS_METAINFO")]
		READ_METAINFO,
		[Version (deprecated = true, replacement = "LOAD_OS_DESKTOP_FILES")]
		READ_DESKTOP_FILES
	}
}
