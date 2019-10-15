namespace AppStream {
	[CCode (cheader_filename = "appstream.h", cprefix = "AS_PROVIDED_KIND_", has_type_id = false)]
	public enum ProvidedKind {
		[Version (deprecated = true, replacement = "MEDIATYPE")]
		MIMETYPE
	}
}
