# ImageApp fixtures

The sample embeds two tiny deterministic raster fixtures and materializes
them in the user's temporary directory at startup: a transparent 3 × 2 PNG
and a 5 × 3 JPEG. This keeps the sample self-contained while ensuring that
the public application code still exercises ordinary file-backed
`ImageSource` values.
