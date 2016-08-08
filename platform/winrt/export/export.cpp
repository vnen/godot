/*************************************************************************/
/*  export.cpp                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "version.h"
#include "export.h"
#include "object.h"
#include "tools/editor/editor_import_export.h"
#include "tools/editor/editor_node.h"
#include "platform/winrt/logo.h"
#include "os/file_access.h"
#include "io/zip.h"
#include "io/unzip.h"
#include "io/zip_io.h"
#include "io/sha256.h"
#include "io/base64.h"
#include "bind/core_bind.h"

class AppxPackager {

	enum {
		FILE_HEADER_MAGIC = 0x04034b50,
		DATA_DESCRIPTOR_MAGIC = 0x08074b50,
		CENTRAL_DIR_MAGIC = 0x02014b50,
		END_OF_CENTRAL_DIR_MAGIC = 0x06054b50,
		ZIP64_END_OF_CENTRAL_DIR_MAGIC = 0x06064b50,
		ZIP64_END_DIR_LOCATOR_MAGIC = 0x07064b50,
		ZIP64_HEADER_ID = 0x0001,
		ZIP_VERSION = 0x2d,
		GENERAL_PURPOSE = 0x08,
		BASE_FILE_HEADER_SIZE = 30,
		DATA_DESCRIPTOR_SIZE = 24,
		BASE_CENTRAL_DIR_SIZE = 46,
		EXTRA_FIELD_LENGTH = 0x1c,
		ZIP64_HEADER_SIZE = 0x18,
		ZIP64_END_OF_CENTRAL_DIR_SIZE = 44,
		END_OF_CENTRAL_DIR_SIZE = 42,
		BLOCK_SIZE = 65536,
	};

	struct BlockHash {

		String base64_hash;
		size_t compressed_size;
	};

	struct FileMeta {

		String name;
		int lfh_size;
		bool compressed;
		size_t compressed_size;
		size_t uncompressed_size;
		Vector<BlockHash> hashes;
		uLong file_crc32;
		ZPOS64_T zip_offset;
	};

	EditorProgress *progress;
	FileAccess *package;
	String tmp_blockmap_file_name;
	String tmp_content_types_file_name;

	Set<String> mime_types;

	Vector<FileMeta> file_metadata;

	ZPOS64_T central_dir_offset;
	ZPOS64_T end_of_central_dir_offset;
	size_t central_dir_size;

	String hash_block(uint8_t* p_block_data, size_t p_block_len);

	void make_block_map();
	void make_content_types();

	int write_file_header(String p_name, bool p_compress);
	int write_file_descriptor(uint32_t p_crc32, size_t p_compressed_size, size_t p_uncompressed_size);
	int write_central_dir_header(const FileMeta p_file);
	void write_zip64_end_of_central_record();
	void write_end_of_central_record();

	String content_type(String p_extension);

public:

	void init(FileAccess* p_fa, EditorProgress* p_progress);
	void add_file(String p_file_name, const uint8_t* p_buffer, size_t p_len, int p_file_no, int p_total_files, bool p_compress = false);
	void finish();
};

String AppxPackager::hash_block(uint8_t * p_block_data, size_t p_block_len) {

	char hash[32];
	char base64[45];

	sha256_context ctx;
	sha256_init(&ctx);
	sha256_hash(&ctx, p_block_data, p_block_len);
	sha256_done(&ctx, (uint8_t*)hash);

	base64_encode(base64, hash, 32);
	base64[44] = '\0';

	return String(base64);
}

void AppxPackager::make_block_map() {

	FileAccess* tmp_file = FileAccess::open(tmp_blockmap_file_name, FileAccess::WRITE);

	tmp_file->store_string("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>");
	tmp_file->store_string("<BlockMap xmlns=\"http://schemas.microsoft.com/appx/2010/blockmap\" HashMethod=\"http://www.w3.org/2001/04/xmlenc#sha256\">");

	for (int i = 0; i < file_metadata.size(); i++) {

		FileMeta file = file_metadata[i];

		tmp_file->store_string(
			"<File Name=\"" + file.name.replace("/", "\\")
			+ "\" Size=\"" + itos(file.uncompressed_size)
			+ "\" LfhSize=\"" + itos(file.lfh_size) + "\">");


		for (int j = 0; j < file.hashes.size(); j++) {

			tmp_file->store_string("<Block Hash=\""
				+ file.hashes[j].base64_hash + "\" ");
			if (file.compressed)
				tmp_file->store_string("Size=\"" + itos(file.hashes[j].compressed_size) + "\" ");
			tmp_file->store_string("/>");
		}

		tmp_file->store_string("</File>");
	}

	tmp_file->store_string("</BlockMap>");

	tmp_file->close();
	memdelete(tmp_file);
	tmp_file = NULL;
}

String AppxPackager::content_type(String p_extension) {

	if (p_extension == "png")
		return "image/png";
	else if (p_extension == "jpg")
		return "image/jpg";
	else if (p_extension == "xml")
		return "application/xml";
	else if (p_extension == "exe" || p_extension == "dll")
		return "application/x-msdownload";
	else
		return "application/octet-stream";
}

void AppxPackager::make_content_types() {

	FileAccess* tmp_file = FileAccess::open(tmp_content_types_file_name, FileAccess::WRITE);

	tmp_file->store_string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
	tmp_file->store_string("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">");

	Map<String, String>	types;

	for (int i = 0; i < file_metadata.size(); i++) {

		String ext = file_metadata[i].name.extension();

		if (types.has(ext)) continue;

		types[ext] = content_type(ext);

		tmp_file->store_string("<Default Extension=\"" + ext +
			"\" ContentType=\""
			+ types[ext] + "\" />");
	}

	// Override for manifest and block map
	tmp_file->store_string("<Override PartName=\"/AppxManifest.xml\" ContentType=\"application/vnd.ms-appx.manifest+xml\" />");
	tmp_file->store_string("<Override PartName=\"/AppxBlockMap.xml\" ContentType=\"application/vnd.ms-appx.blockmap+xml\" />");

	tmp_file->store_string("</Types>");

	tmp_file->close();
	memdelete(tmp_file);
	tmp_file = NULL;
}

int AppxPackager::write_file_header(String p_name, bool p_compress) {

	Vector<uint8_t> buf;
	buf.resize(BASE_FILE_HEADER_SIZE + p_name.length());

	int offs = 0;
	// Write magic
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (FILE_HEADER_MAGIC >> (i * 8)) & 0xFF;
	}

	// Version
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP_VERSION >> (i * 8)) & 0xFF;
	}

	// Special flag
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (GENERAL_PURPOSE >> (i * 8)) & 0xFF;
	}

	// Compression
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (((uint16_t)(p_compress ? 0x0008 : 0x0000)) >> (i * 8)) & 0xFF;
	}

	// Empty header data
	for (int i = 0; i < 16; i++) {
		buf[offs++] = 0;
	}

	// File name length
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (p_name.length() >> (i * 8)) & 0xFF;
	}

	// Extra data length
	for (int i = 0; i < 2; i++) {
		buf[offs++] = 0;
	}

	// File name
	for (int i = 0; i < p_name.length(); i++) {
		buf[offs++] = p_name.utf8().get(i);
	}

	// Done!
	package->store_buffer(buf.ptr(), buf.size());

	return buf.size();
}

int AppxPackager::write_file_descriptor(uint32_t p_crc32, size_t p_compressed_size, size_t p_uncompressed_size) {

	Vector<uint8_t> buf;
	buf.resize(DATA_DESCRIPTOR_SIZE);

	int offs = 0;

	// Write magic
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (DATA_DESCRIPTOR_MAGIC >> (i * 8)) & 0xFF;
	}

	// CRC
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (p_crc32 >> (i * 8)) & 0xFF;
	}

	// Compressed size
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (p_compressed_size >> (i * 8)) & 0xFF;
	}

	// Uncompressed size
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (p_uncompressed_size >> (i * 8)) & 0xFF;
	}

	// Done!
	package->store_buffer(buf.ptr(), buf.size());

	return buf.size();
}

int AppxPackager::write_central_dir_header(const FileMeta p_file) {

	Vector<uint8_t> buf;
	buf.resize(BASE_CENTRAL_DIR_SIZE + p_file.name.length() + EXTRA_FIELD_LENGTH);

	int offs = 0;

	// Write magic
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (CENTRAL_DIR_MAGIC >> (i * 8)) & 0xFF;
	}
	// Version (twice)
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP_VERSION >> (i * 8)) & 0xFF;
	}
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP_VERSION >> (i * 8)) & 0xFF;
	}
	// General purpose flag
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (GENERAL_PURPOSE >> (i * 8)) & 0xFF;
	}

	// Compression
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (((uint16_t)(p_file.compressed ? 8 : 0)) >> i * 8) & 0xFF;
	}

	// Modification date/time
	for (int i = 0; i < 4; i++) {
		buf[offs++] = 0;
	}

	// Crc-32
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (p_file.file_crc32 >> (i * 8)) & 0xFF;
	}

	// File sizes (will be in extra field)
	for (int i = 0; i < 8; i++) {
		buf[offs++] = 0xFF;
	}

	// File name length
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (p_file.name.length() >> (i * 8)) & 0xFF;
	}

	// Extra field length
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (EXTRA_FIELD_LENGTH >> (i * 8)) & 0xFF;
	}

	// Comment length
	for (int i = 0; i < 2; i++) {
		buf[offs++] = 0;
	}

	// Disk number start, internal/external file attributes
	for (int i = 0; i < 8; i++) {
		buf[offs++] = 0;
	}

	// Relative offset (will be on extra field)
	for (int i = 0; i < 4; i++) {
		buf[offs++] = 0xFF;
	}

	// File name
	for (int i = 0; i < p_file.name.length(); i++) {
		buf[offs++] = p_file.name.utf8().get(i);
	}

	// Zip64 extra field
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP64_HEADER_ID >> (i * 8)) & 0xFF;
	}
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP64_HEADER_SIZE >> (i * 8)) & 0xFF;
	}

	// Original size
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (p_file.uncompressed_size >> (i * 8)) & 0xFF;
	}
	// Compressed size
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (p_file.compressed_size >> (i * 8)) & 0xFF;
	}
	// File offset
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (p_file.zip_offset >> (i * 8)) & 0xFF;
	}

	// Done!
	package->store_buffer(buf.ptr(), buf.size());

	return buf.size();
}

void AppxPackager::write_zip64_end_of_central_record() {

	Vector<uint8_t> buf;
	buf.resize(ZIP64_END_OF_CENTRAL_DIR_SIZE + 12); // Size plus magic

	int offs = 0;

	// Write magic
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (ZIP64_END_OF_CENTRAL_DIR_MAGIC >> (i * 8)) & 0xFF;
	}

	// Size of this record
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)ZIP64_END_OF_CENTRAL_DIR_SIZE) >> (i * 8)) & 0xFF;
	}

	// Version (yes, twice)
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP_VERSION >> (i * 8)) & 0xFF;
	}
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP_VERSION >> (i * 8)) & 0xFF;
	}

	// Disk number
	for (int i = 0; i < 8; i++) {
		buf[offs++] = 0;
	}

	// Number of entries (total and per disk)
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)file_metadata.size()) >> (i * 8)) & 0xFF;
	}
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)file_metadata.size()) >> (i * 8)) & 0xFF;
	}

	// Size of central dir
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)central_dir_size) >> (i * 8)) & 0xFF;
	}

	// Central dir offset
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (central_dir_offset >> (i * 8)) & 0xFF;
	}

	// Done!
	package->store_buffer(buf.ptr(), buf.size());
}

void AppxPackager::write_end_of_central_record() {

	Vector<uint8_t> buf;
	buf.resize(END_OF_CENTRAL_DIR_SIZE);

	int offs = 0;

	// Write magic for zip64 centra dir locator
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (ZIP64_END_DIR_LOCATOR_MAGIC >> (i * 8)) & 0xFF;
	}

	// Disk number
	for (int i = 0; i < 4; i++) {
		buf[offs++] = 0;
	}

	// Relative offset
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (end_of_central_dir_offset >> (i * 8)) & 0xFF;
	}

	// Number of disks
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (1 >> (i * 8)) & 0xFF;
	}

	// Write magic for end central dir
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (END_OF_CENTRAL_DIR_MAGIC >> (i * 8)) & 0xFF;
	}

	// Dummy stuff for Zip64
	for (int i = 0; i < 16; i++) {
		buf[offs++] = 0xFF;
	}
	// Size of comments
	for (int i = 0; i < 2; i++) {
		buf[offs++] = 0;
	}

	// Done!
	package->store_buffer(buf.ptr(), buf.size());
}

void AppxPackager::init(FileAccess * p_fa, EditorProgress* p_progress) {

	progress = p_progress;
	package = p_fa;
	central_dir_offset = 0;
	end_of_central_dir_offset = 0;
	tmp_blockmap_file_name = EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmpblockmap.xml";
	tmp_content_types_file_name = EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmpcontenttypes.xml";
}

void AppxPackager::add_file(String p_file_name, const uint8_t * p_buffer, size_t p_len, int p_file_no, int p_total_files, bool p_compress) {

	if (p_file_no >= 1 && p_total_files >= 1) {
		progress->step("File: " + p_file_name, 3 + p_file_no * 100 / p_total_files);
	}

	FileMeta meta;
	meta.name = p_file_name;
	meta.uncompressed_size = p_len;
	meta.compressed_size = p_len;
	meta.compressed = p_compress;
	meta.zip_offset = package->get_pos();


	// Create file header
	meta.lfh_size = write_file_header(p_file_name, p_compress);

	// Data for compression
	z_stream strm;
	FileAccess* strm_f = NULL;
	Vector<uint8_t> strm_in;
	strm_in.resize(BLOCK_SIZE);
	Vector<uint8_t> strm_out;

	if (p_compress) {

		strm.zalloc = zipio_alloc;
		strm.zfree = zipio_free;
		strm.opaque = &strm_f;

		strm_out.resize(BLOCK_SIZE + 8);

		deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
	}

	int step = 0;

	while (p_len - step > 0) {

		size_t block_size = (p_len - step) > BLOCK_SIZE ? BLOCK_SIZE : (p_len - step);

		for (int i = 0; i < block_size; i++) {
			strm_in[i] = p_buffer[step + i];
		}

		BlockHash bh;
		bh.base64_hash = hash_block(strm_in.ptr(), block_size);

		if (p_compress) {

			strm.avail_in = block_size;
			strm.avail_out = strm_out.size();
			strm.next_in = strm_in.ptr();
			strm.next_out = strm_out.ptr();

			int total_out_before = strm.total_out;

			deflate(&strm, Z_FULL_FLUSH);
			bh.compressed_size = strm.total_out - total_out_before;

			package->store_buffer(strm_out.ptr(), strm.total_out - total_out_before);

		} else {
			bh.compressed_size = block_size;
			package->store_buffer(strm_in.ptr(), block_size);
		}

		meta.hashes.push_back(bh);

		step += block_size;
	}

	if (p_compress) {

		strm.avail_in = 0;
		strm.avail_out = strm_out.size();
		strm.next_in = strm_in.ptr();
		strm.next_out = strm_out.ptr();

		int total_out_before = strm.total_out;

		deflate(&strm, Z_FINISH);

		package->store_buffer(strm_out.ptr(), strm.total_out - total_out_before);

		deflateEnd(&strm);
		meta.compressed_size = strm.total_out;

	} else {

		meta.compressed_size = p_len;
	}

	// Calculate file CRC-32
	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, p_buffer, p_len);
	meta.file_crc32 = crc;

	// Create data descriptor
	write_file_descriptor(crc, meta.compressed_size, meta.uncompressed_size);

	file_metadata.push_back(meta);
}

void AppxPackager::finish() {

	// Create and add block map file
	progress->step("Creating block map...", 103);
	make_block_map();
	FileAccess* blockmap_file = FileAccess::open(tmp_blockmap_file_name, FileAccess::READ);
	Vector<uint8_t> blockmap_buffer;
	blockmap_buffer.resize(blockmap_file->get_len());

	blockmap_file->get_buffer(blockmap_buffer.ptr(), blockmap_buffer.size());

	add_file("AppxBlockMap.xml", blockmap_buffer.ptr(), blockmap_buffer.size(), -1, -1, true);

	blockmap_file->close();
	memdelete(blockmap_file);
	blockmap_file = NULL;

	// Add content types
	progress->step("Setting content types...", 104);
	make_content_types();
	
	FileAccess* types_file = FileAccess::open(tmp_content_types_file_name, FileAccess::READ);
	Vector<uint8_t> types_buffer;
	types_buffer.resize(types_file->get_len());

	types_file->get_buffer(types_buffer.ptr(), types_buffer.size());

	add_file("[Content_Types].xml", types_buffer.ptr(), types_buffer.size(), -1, -1, true);

	types_file->close();
	memdelete(types_file);
	types_file = NULL;


	// Central directory
	progress->step("Finishing package...", 105);
	central_dir_offset = package->get_pos();
	central_dir_size = 0;
	for (int i = 0; i < file_metadata.size(); i++) {

		central_dir_size += write_central_dir_header(file_metadata[i]);
	}

	// End record
	end_of_central_dir_offset = package->get_pos();
	write_zip64_end_of_central_record();
	write_end_of_central_record();

	package->close();
	memdelete(package);
	package = NULL;
}

class EditorExportPlatformWinrt : public EditorExportPlatform {

	OBJ_TYPE(EditorExportPlatformWinrt, EditorExportPlatform);

	struct AppxExportData {

		zipFile appx;
		EditorProgress* ep;
	};

	Ref<ImageTexture> logo;

	bool export_x86;
	bool export_x64;
	bool export_arm;

	bool is_debug;

	String custom_release_package;
	String custom_debug_package;

	static Error save_appx_file(void *p_userdata, const String& p_path, const Vector<uint8_t>& p_data, int p_file, int p_total);
	static bool _should_compress_asset(const String& p_path, const Vector<uint8_t>& p_data);

protected:

	bool _set(const StringName& p_name, const Variant& p_value);
	bool _get(const StringName& p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:

	virtual String get_name() const { return "Windows Universal"; }
	virtual ImageCompression get_image_compression() const { return IMAGE_COMPRESSION_ETC1; }
	virtual Ref<Texture> get_logo() const { return logo; }

	virtual bool can_export(String *r_error = NULL) const;
	virtual String get_binary_extension() const { return "appx"; }

	virtual Error export_project(const String& p_path, bool p_debug, int p_flags = 0);

	EditorExportPlatformWinrt();
	~EditorExportPlatformWinrt();
};

Error EditorExportPlatformWinrt::save_appx_file(void * p_userdata, const String & p_path, const Vector<uint8_t>& p_data, int p_file, int p_total) {

	AppxPackager *packager = (AppxPackager*)p_userdata;
	String dst_path = p_path.replace_first("res://", "");

	packager->add_file(dst_path, p_data.ptr(), p_data.size(), p_file, p_total, _should_compress_asset(p_path, p_data));

	return OK;
}

bool EditorExportPlatformWinrt::_should_compress_asset(const String & p_path, const Vector<uint8_t>& p_data) {

	/* TODO: This was copied verbatim from Android export. It should be
	 * refactored to the parent class and also be used for .zip export.
	 */

	 /*
	 *  By not compressing files with little or not benefit in doing so,
	 *  a performance gain is expected at runtime. Moreover, if the APK is
	 *  zip-aligned, assets stored as they are can be efficiently read by
	 *  Android by memory-mapping them.
	 */

	 // -- Unconditional uncompress to mimic AAPT plus some other

	static const char* unconditional_compress_ext[] = {
		// From https://github.com/android/platform_frameworks_base/blob/master/tools/aapt/Package.cpp
		// These formats are already compressed, or don't compress well:
		".jpg", ".jpeg", ".png", ".gif",
		".wav", ".mp2", ".mp3", ".ogg", ".aac",
		".mpg", ".mpeg", ".mid", ".midi", ".smf", ".jet",
		".rtttl", ".imy", ".xmf", ".mp4", ".m4a",
		".m4v", ".3gp", ".3gpp", ".3g2", ".3gpp2",
		".amr", ".awb", ".wma", ".wmv",
		// Godot-specific:
		".webp", // Same reasoning as .png
		".cfb", // Don't let small config files slow-down startup
				// Trailer for easier processing
				NULL
	};

	for (const char** ext = unconditional_compress_ext; *ext; ++ext) {
		if (p_path.to_lower().ends_with(String(*ext))) {
			return false;
		}
	}

	// -- Compressed resource?

	if (p_data.size() >= 4 && p_data[0] == 'R' && p_data[1] == 'S' && p_data[2] == 'C' && p_data[3] == 'C') {
		// Already compressed
		return false;
	}

	// --- TODO: Decide on texture resources according to their image compression setting

	return true;
}

bool EditorExportPlatformWinrt::_set(const StringName& p_name, const Variant& p_value) {

	String n = p_name;

	if (n == "architecture/arm")
		export_arm = p_value;
	else if (n == "architecture/x86")
		export_x86 = p_value;
	else if (n == "architecture/x64")
		export_x64 = p_value;
	else if (n == "custom_package/debug")
		custom_debug_package = p_value;
	else if (n == "custom_package/release")
		custom_release_package = p_value;
	else return false;
	return true;
}

bool EditorExportPlatformWinrt::_get(const StringName& p_name, Variant &r_ret) const {

	String n = p_name;

	if (n == "architecture/arm")
		r_ret = export_arm;
	else if (n == "architecture/x86")
		r_ret = export_x86;
	else if (n == "architecture/x64")
		r_ret = export_x64;
	else if (n == "custom_package/debug")
		r_ret = custom_debug_package;
	else if (n == "custom_package/release")
		r_ret = custom_release_package;
	else return false;
	return true;
}

void EditorExportPlatformWinrt::_get_property_list(List<PropertyInfo>* p_list) const {

	p_list->push_back(PropertyInfo(Variant::STRING, "custom_package/debug", PROPERTY_HINT_GLOBAL_FILE, "appx"));
	p_list->push_back(PropertyInfo(Variant::STRING, "custom_package/release", PROPERTY_HINT_GLOBAL_FILE, "appx"));

	p_list->push_back(PropertyInfo(Variant::BOOL, "architecture/arm"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "architecture/x86"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "architecture/x64"));
}

bool EditorExportPlatformWinrt::can_export(String * r_error) const {

	String err;
	bool valid = true;

	if (!exists_export_template("winrt_x86_debug.appx") || !exists_export_template("winrt_x86_release.appx")) {
		valid = false;
		err += "No export templates found.\nDownload and install export templates.\n";
	}

	if (custom_debug_package != "" && !FileAccess::exists(custom_debug_package)) {
		valid = false;
		err += "Custom debug package not found.\n";
	}

	if (custom_release_package != "" && !FileAccess::exists(custom_release_package)) {
		valid = false;
		err += "Custom release package not found.\n";
	}

	if (r_error)
		*r_error = err;

	return valid;
}

Error EditorExportPlatformWinrt::export_project(const String & p_path, bool p_debug, int p_flags) {

	String src_appx;

	EditorProgress ep("export", "Exporting for Windows Universal", 105);

	if (is_debug)
		src_appx = custom_debug_package;
	else
		src_appx = custom_release_package;

	if (src_appx == "") {
		String err;
		if (p_debug) {
			src_appx = find_export_template("winrt_x86_debug.appx", &err);
		} else {
			src_appx = find_export_template("winrt_x86_release.appx", &err);
		}
		if (src_appx == "") {
			EditorNode::add_io_error(err);
			return ERR_FILE_NOT_FOUND;
		}
	}

	Error err = OK;
	FileAccess *fa_pack = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	AppxPackager packager;
	packager.init(fa_pack, &ep);

	FileAccess *src_f = NULL;
	zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

	ep.step("Creating package", 0);

	unzFile pkg = unzOpen2(src_appx.utf8().get_data(), &io);

	if (!pkg) {

		EditorNode::add_io_error("Could not find template appx to export:\n" + src_appx);
		return ERR_FILE_NOT_FOUND;
	}

	int ret = unzGoToFirstFile(pkg);

	while (ret == UNZ_OK) {

		// get file name
		unz_file_info info;
		char fname[16834];
		ret = unzGetCurrentFileInfo(pkg, &info, fname, 16834, NULL, 0, NULL, 0);

		String path = fname;

		if (path.ends_with("/")) {
			// Ignore directories
			ret = unzGoToNextFile(pkg);
			continue;
		}

		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		//read
		unzOpenCurrentFile(pkg);
		unzReadCurrentFile(pkg, data.ptr(), data.size());
		unzCloseCurrentFile(pkg);

		print_line("ADDING: " + path);

		packager.add_file(path, data.ptr(), data.size(), -1, -1, _should_compress_asset(path, data));

		ret = unzGoToNextFile(pkg);
	}

	ep.step("Adding Files..", 1);
	Vector<String> cl;

	gen_export_flags(cl, p_flags);

	err = export_project_files(save_appx_file, &packager, false);

	unzClose(pkg);

	packager.finish();

	return OK;
}

EditorExportPlatformWinrt::EditorExportPlatformWinrt() {

	Image img(_winrt_logo);
	logo = Ref<ImageTexture>(memnew(ImageTexture));
	logo->create_from_image(img);
}

EditorExportPlatformWinrt::~EditorExportPlatformWinrt() {}


void register_winrt_exporter() {

	Ref<EditorExportPlatformWinrt> exporter = Ref<EditorExportPlatformWinrt>(memnew(EditorExportPlatformWinrt));
	EditorImportExport::get_singleton()->add_export_platform(exporter);
}
