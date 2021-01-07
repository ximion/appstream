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
}
