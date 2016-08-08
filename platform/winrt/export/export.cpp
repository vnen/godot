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

	Set<String> mime_types;

	Vector<FileMeta> file_metadata;

	ZPOS64_T central_dir_offset;
	ZPOS64_T end_of_central_dir_offset;
	size_t central_dir_size;

	String hash_block(uint8_t* p_block_data, size_t p_block_len);

	void make_block_map();

	void make_file_header(uint8_t *p_buf, String p_name, bool p_compress);
	void make_file_descriptor(uint8_t* p_buf, uint32_t p_crc32, size_t p_compressed_size, size_t p_uncompressed_size);
	void make_central_dir_header(uint8_t* p_buf, const FileMeta p_file);
	void write_zip64_end_of_central_record();
	void write_end_of_central_record();

public:

	void init(FileAccess* p_fa, EditorProgress* p_progress);
	void add_file(String p_file_name, const uint8_t* p_buffer, size_t p_len, bool p_compress = false, bool p_make_hash = true);
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
			"<File Name=\"" + file.name.replace("/","\\")
			+ "\" Size=\"" + itos(file.uncompressed_size)
			+ "\" LfhSize=\"" + itos(file.lfh_size) + "\">");


		// <Block Hash="HKjnnQ/pWYOjvpXHoeXz0LPUHuFH7DL5jujS6+f+bsI=" Size="853"/>
		for (int j = 0; j < file.hashes.size(); j++) {

			tmp_file->store_string("<Block Hash=\""
				+ file.hashes[j].base64_hash + "\" Size=\""
				+ itos(file.hashes[j].compressed_size) + "\"/>");
		}

		tmp_file->store_string("</File>");
	}

	tmp_file->store_string("</BlockMap>");

	tmp_file->close();
	tmp_file = NULL;
}

void AppxPackager::make_file_header(uint8_t *p_buf, String p_name, bool p_compress) {

	int name_len = p_name.length();

	// Write magic
	p_buf[0] = 0x50;
	p_buf[1] = 0x4b;
	p_buf[2] = 0x03;
	p_buf[3] = 0x04;

	// Version
	p_buf[4] = 0x2d;
	p_buf[5] = 0x00;

	// Special flag
	p_buf[6] = 0x08;
	p_buf[7] = 0x00;

	// Compression
	p_buf[8] = p_compress ? 0x08 : 0x00;
	p_buf[9] = 0x00;

	// Empty header data
	for (int i = 10; i < 26; i++) {
		p_buf[i] = 0x00;
	}

	// File name length (little-endian)
	p_buf[26] = name_len & 0xFF;
	p_buf[27] = (name_len >> 8) & 0xFF;

	// Extra data length
	p_buf[28] = 0x00;
	p_buf[29] = 0x00;

	// File name
	for (int i = 0; i < name_len; i++) {
		p_buf[i + 30] = p_name.utf8().get(i);
	}

	// Done!
}

void AppxPackager::make_file_descriptor(uint8_t* p_buf, uint32_t p_crc32, size_t p_compressed_size, size_t p_uncompressed_size) {

	// Write magic
	for (int i = 0; i < 4; i++) {
		p_buf[i] = (DATA_DESCRIPTOR_MAGIC >> (i*8)) & 0xFF;
	}

	// CRC
	for (int i = 0; i < 4; i++) {
		p_buf[i + 4] = (p_crc32 >> (i*8)) & 0xFF;
	}

	// Compressed size
	for (int i = 0; i < 8; i++) {
		p_buf[i + 8] = (p_compressed_size >> (i*8)) & 0xFF;
	}

	// Uncompressed size
	for (int i = 0; i < 8; i++) {
		p_buf[i + 16] = (p_uncompressed_size >> (i*8)) & 0xFF;
	}

	// Done!
}

void AppxPackager::make_central_dir_header(uint8_t* p_buf, const FileMeta p_file) {

	int offs = 0;

	// Write magic
	for (int i = 0; i < 4; i++) {
		p_buf[offs++] = (CENTRAL_DIR_MAGIC >> (i*8)) & 0xFF;
	}
	// Version (twice)
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (ZIP_VERSION >> (i*8)) & 0xFF;
	}
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (ZIP_VERSION >> (i*8)) & 0xFF;
	}
	// General purpose flag
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (GENERAL_PURPOSE >> (i * 8)) & 0xFF;
	}

	// Compression
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (((uint16_t)(p_file.compressed ? 8 : 0)) >> i * 8) & 0xFF;
	}

	// Modification date/time
	for (int i = 0; i < 4; i++) {
		p_buf[offs++] = 0;
	}

	// Crc-32
	for (int i = 0; i < 4; i++) {
		p_buf[offs++] = (p_file.file_crc32 >> (i*8)) & 0xFF;
	}

	// File sizes (will be in extra field)
	for (int i = 0; i < 8; i++) {
		p_buf[offs++] = 0xFF;
	}

	// File name length
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (p_file.name.length() >> (i*8)) & 0xFF;
	}

	// Extra field length
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (EXTRA_FIELD_LENGTH >> (i*8)) & 0xFF;
	}

	// Comment length
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = 0;
	}

	// Disk number start, internal/external file attributes
	for (int i = 0; i < 8; i++) {
		p_buf[offs++] = 0;
	}

	// Relative offset (will be on extra field)
	for (int i = 0; i < 4; i++) {
		p_buf[offs++] = 0xFF;
	}

	// File name
	for (int i = 0; i < p_file.name.length(); i++) {
		p_buf[offs++] = p_file.name.utf8().get(i);
	}

	// Zip64 extra field
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (ZIP64_HEADER_ID >> (i*8)) & 0xFF;
	}
	for (int i = 0; i < 2; i++) {
		p_buf[offs++] = (ZIP64_HEADER_SIZE >> (i*8)) & 0xFF;
	}

	// Original size
	for (int i = 0; i < 8; i++) {
		p_buf[offs++] = (p_file.uncompressed_size >> (i*8)) & 0xFF;
	}
	// Compressed size
	for (int i = 0; i < 8; i++) {
		p_buf[offs++] = (p_file.compressed_size >> (i*8)) & 0xFF;
	}
	// File offset
	for (int i = 0; i < 8; i++) {
		p_buf[offs++] = (p_file.zip_offset >> (i*8)) & 0xFF;
	}

	// Done!
}

void AppxPackager::write_zip64_end_of_central_record() {

	Vector<uint8_t> buf;
	buf.resize(ZIP64_END_OF_CENTRAL_DIR_SIZE + 12); // Size plus magic

	int offs = 0;

	// Write magic
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (ZIP64_END_OF_CENTRAL_DIR_MAGIC >> (i*8)) & 0xFF;
	}

	// Size of this record
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)ZIP64_END_OF_CENTRAL_DIR_SIZE) >> (i*8)) & 0xFF;
	}

	// Version (yes, twice)
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP_VERSION >> (i*8)) & 0xFF;
	}
	for (int i = 0; i < 2; i++) {
		buf[offs++] = (ZIP_VERSION >> (i*8)) & 0xFF;
	}

	// Disk number
	for (int i = 0; i < 8; i++) {
		buf[offs++] = 0;
	}

	// Number of entries (total and per disk)
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)file_metadata.size()) >> (i*8)) & 0xFF;
	}
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)file_metadata.size()) >> (i*8)) & 0xFF;
	}

	// Size of central dir
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (((uint64_t)central_dir_size) >> (i*8)) & 0xFF;
	}

	// Central dir offset
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (central_dir_offset >> (i*8)) & 0xFF;
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
		buf[offs++] = (ZIP64_END_DIR_LOCATOR_MAGIC >> (i*8)) & 0xFF;
	}

	// Disk number
	for (int i = 0; i < 4; i++) {
		buf[offs++] = 0;
	}

	// Relative offset
	for (int i = 0; i < 8; i++) {
		buf[offs++] = (end_of_central_dir_offset >> (i*8)) & 0xFF;
	}

	// Number of disks
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (1 >> (i*8)) & 0xFF;
	}

	// Write magic for end central dir
	for (int i = 0; i < 4; i++) {
		buf[offs++] = (END_OF_CENTRAL_DIR_MAGIC >> (i*8)) & 0xFF;
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
}

void AppxPackager::add_file(String p_file_name, const uint8_t * p_buffer, size_t p_len, bool p_compress, bool p_make_hash) {

	FileMeta meta;
	meta.name = p_file_name;
	meta.uncompressed_size = p_len;
	meta.compressed_size = p_len;
	meta.compressed = p_compress;
	meta.zip_offset = package->get_pos();


	// Create file header
	Vector<uint8_t> file_header;
	file_header.resize(BASE_FILE_HEADER_SIZE + p_file_name.length());
	make_file_header(file_header.ptr(), p_file_name, p_compress);
	meta.lfh_size = file_header.size();

	package->store_buffer(file_header.ptr(), file_header.size());


	if (p_compress) {

		z_stream strm;
		strm.zalloc = zipio_alloc;
		strm.zfree = zipio_free;
		FileAccess* strm_f = NULL;
		strm.opaque = &strm_f;

		int step = 0;

		Vector<uint8_t> strm_in;
		strm_in.resize(BLOCK_SIZE);
		Vector<uint8_t> strm_out;
		strm_out.resize(BLOCK_SIZE + 8);


		deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);

		while (p_len - step > 0) {

			size_t block_size = (p_len - step) > BLOCK_SIZE ? BLOCK_SIZE : (p_len - step);

			for (int i = 0; i < block_size; i++) {
				strm_in[i] = p_buffer[step + i];
			}

			strm.avail_in = block_size;
			strm.avail_out = strm_out.size();
			strm.next_in = strm_in.ptr();
			strm.next_out = strm_out.ptr();

			int total_out_before = strm.total_out;

			deflate(&strm, (p_len - (step + block_size)) > 0 ? Z_SYNC_FLUSH : Z_FINISH);

			BlockHash bh;
			bh.base64_hash = hash_block(strm_in.ptr(), block_size);
			bh.compressed_size = strm.total_out - total_out_before;

			package->store_buffer(strm_out.ptr(), strm.total_out - total_out_before);

			meta.hashes.push_back(bh);

			step += block_size;
		}

		deflateEnd(&strm);

		meta.compressed_size = strm.total_out;

	} else {

		// Make block hashes
		if (p_make_hash) {
			Vector<uint8_t> block;
			block.resize(BLOCK_SIZE);
			size_t step = 0;

			while (p_len - step > 0) {

				size_t block_size = (p_len - step) > BLOCK_SIZE ? BLOCK_SIZE : (p_len - step);

				for (int i = 0; i < block_size; i++) {
					block[i] = p_buffer[step + i];
				}

				BlockHash bh;
				bh.base64_hash = hash_block(block.ptr(), block_size);
				bh.compressed_size = block_size;

				meta.hashes.push_back(bh);

				step += block_size;
			}
		}

		// Store file
		package->store_buffer(p_buffer, p_len);
	}

	// Calculate file CRC-32
	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, p_buffer, p_len);
	meta.file_crc32 = crc;

	// Create data descriptor
	Vector<uint8_t> file_descriptor;
	file_descriptor.resize(DATA_DESCRIPTOR_SIZE);
	make_file_descriptor(file_descriptor.ptr(), crc, meta.compressed_size, meta.uncompressed_size);

	package->store_buffer(file_descriptor.ptr(), file_descriptor.size());

	file_metadata.push_back(meta);
}

void AppxPackager::finish() {

	// Create and add block map file
	make_block_map();
	FileAccess* blockmap_file = FileAccess::open(tmp_blockmap_file_name, FileAccess::READ);
	Vector<uint8_t> blockmap_buffer;
	blockmap_buffer.resize(blockmap_file->get_len());

	blockmap_file->get_buffer(blockmap_buffer.ptr(), blockmap_buffer.size());

	add_file("AppxBlockMap.xml", blockmap_buffer.ptr(), blockmap_buffer.size(), true, false);

	blockmap_file->close();
	blockmap_file = NULL;


	// Add content types
	FileAccess* types_file = FileAccess::open(EditorSettings::get_singleton()->get_settings_path() + "/tmp/[Content_Types].xml", FileAccess::READ);
	Vector<uint8_t> types_buffer;
	types_buffer.resize(types_file->get_len());

	types_file->get_buffer(types_buffer.ptr(), types_buffer.size());

	add_file("[Content_Types].xml", types_buffer.ptr(), types_buffer.size(), true, false);

	types_file->close();
	types_file = NULL;


	// Central directory
	central_dir_offset = package->get_pos();
	central_dir_size = 0;
	for (int i = 0; i < file_metadata.size(); i++) {

		FileMeta file = file_metadata[i];
		Vector<uint8_t> buff;
		buff.resize(BASE_CENTRAL_DIR_SIZE + file.name.length() + EXTRA_FIELD_LENGTH);

		central_dir_size += buff.size();

		make_central_dir_header(buff.ptr(), file);

		package->store_buffer(buff.ptr(), buff.size());
	}

	// End record
	end_of_central_dir_offset = package->get_pos();
	write_zip64_end_of_central_record();
	write_end_of_central_record();

	package->close();
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

	packager->add_file(dst_path, p_data.ptr(), p_data.size(), !p_path.ends_with(".png"));

	return OK;
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

		String file = fname;

		if (file.ends_with("/")) {
			ret = unzGoToNextFile(pkg);
			continue;
		}

		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		//read
		unzOpenCurrentFile(pkg);
		unzReadCurrentFile(pkg, data.ptr(), data.size());
		unzCloseCurrentFile(pkg);

		print_line("ADDING: " + file);

		packager.add_file(file, data.ptr(), data.size(), !file.ends_with(".png"));

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
