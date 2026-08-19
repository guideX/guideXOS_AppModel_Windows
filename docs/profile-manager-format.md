# ProfileManager document format

ProfileManager saves collections in UTF-8 files with the `.gxprofiles`
extension. `File::WriteAllText` writes this format without a BOM. `File::ReadAllText`
accepts and removes one leading UTF-8 BOM before parsing.

Version 1 uses this deterministic line-oriented, length-prefixed structure:

```text
GXPROFILESET 1
profiles <count>
profile
id <unsigned decimal id>
name <UTF-8 byte length>
<exactly that many UTF-8 bytes>
description <UTF-8 byte length>
<exactly that many UTF-8 bytes>
enabled true|false
mode standard|advanced|compatibility
end
```

Structural lines use `LF`; `CRLF` is accepted for structural line endings.
Length-prefixed names and descriptions may contain newlines, `=`, delimiters,
Unicode, and empty text without escaping. Lengths are UTF-8 byte counts.

Profiles are serialized in collection order. IDs are non-zero and unique so
duplicate display names remain unambiguous. An empty collection is valid.

The bounded parser accepts at most 4,096 profiles, 64 KiB for a name, 1 MiB
for a description, and 16 MiB for the complete serialized document. It rejects
unsupported versions, malformed or truncated records, duplicate/invalid IDs,
invalid UTF-8, excessive counts, excessive fields, and trailing content. It
parses into temporary state before replacing the live ProfileManager document.

The serializer fixes the header, field order, mode spelling, boolean spelling,
and profile order. Future formats must use a new version marker.
