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

	AppxExportData *ed = (AppxExportData*)p_userdata;
	String dst_path = p_path;
	dst_path = dst_path.replace_first("res://", "");

	int err = zipOpenNewFileInZip(
		ed->appx,
		dst_path.utf8().get_data(),
		NULL,
		NULL,
		0,
		NULL,
		0,
		NULL,
		Z_NO_COMPRESSION,
		Z_DEFAULT_COMPRESSION
	);

	if (err != ZIP_OK) return ERR_CANT_CREATE;

	err = zipWriteInFileInZip(ed->appx, p_data.ptr(), p_data.size());
	zipCloseFileInZip(ed->appx);
	if (err != ZIP_OK) {
		return ERR_FILE_CANT_WRITE;
	}

	ed->ep->step("File: " + p_path, 3 + p_file * 100 / p_total);

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

	FileAccess *src_f = NULL;
	zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

	ep.step("Creating package", 0);

	unzFile pkg = unzOpen2(src_appx.utf8().get_data(), &io);

	if (!pkg) {

		EditorNode::add_io_error("Could not find template appx to export:\n" + src_appx);
		return ERR_FILE_NOT_FOUND;
	}

	int ret = unzGoToFirstFile(pkg);

	zlib_filefunc_def io2 = io;
	FileAccess *dst_f = NULL;
	io2.opaque = &dst_f;
	String unhashed_path = p_path;//EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmpexport_appx_unhashed.appx";
	zipFile unhashed_appx = zipOpen2(unhashed_path.utf8().get_data(), APPEND_STATUS_CREATE, NULL, &io2);

	while (ret == UNZ_OK) {

		// get file name
		unz_file_info info;
		char fname[16834];
		ret = unzGetCurrentFileInfo(pkg, &info, fname, 16834, NULL, 0, NULL, 0);

		String file = fname;

		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		//read
		unzOpenCurrentFile(pkg);
		unzReadCurrentFile(pkg, data.ptr(), data.size());
		unzCloseCurrentFile(pkg);

		print_line("ADDING: " + file);


		// write
		const bool uncompressed = info.compression_method == 0;

		zipOpenNewFileInZip(unhashed_appx,
			file.utf8().get_data(),
			NULL,
			NULL,
			0,
			NULL,
			0,
			NULL,
			Z_NO_COMPRESSION,
			Z_DEFAULT_COMPRESSION
		);

		zipWriteInFileInZip(unhashed_appx, data.ptr(), data.size());
		zipCloseFileInZip(unhashed_appx);

		ret = unzGoToNextFile(pkg);

	}

	ep.step("Adding Files..", 1);
	Error err = OK;
	Vector<String> cl;

	gen_export_flags(cl, p_flags);

	AppxExportData ed;
	ed.appx = unhashed_appx;
	ed.ep = &ep;

	err = export_project_files(save_appx_file, &ed, false);

	unzClose(pkg);
	zipClose(unhashed_appx, NULL);

	FileAccess* hashing_f = NULL;
	zlib_filefunc_def io_hash = io;
	io_hash.opaque = &hashing_f;
	unzFile hashing_appx = unzOpen2(unhashed_path.utf8().get_data(), &io_hash);
	int base_header_size = 30;

	String block_map_path = EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmpexport_appx_blockmap.xml";
	FileAccess* block_map = FileAccess::open(unhashed_path + "_blockmap.xml", FileAccess::WRITE_READ, &err);

	block_map->store_string("<?xml version=\"1.0\" encoding=\"UTF - 8\" standalone=\"no\"?>\n");
	block_map->store_string("<BlockMap xmlns=\"http://schemas.microsoft.com/appx/2010/blockmap\" HashMethod=\"http://www.w3.org/2001/04/xmlenc#sha256\">\n");


	ep.step("Creating hashes", 103);

	ret = unzGoToFirstFile(hashing_appx);

	while (ret == UNZ_OK) {

		// Get file info
		// get file name
		unz_file_info info;
		char fname[16834];
		ret = unzGetCurrentFileInfo(hashing_appx, &info, fname, 16834, NULL, 0, NULL, 0);

		int header_size = base_header_size + info.size_filename + info.size_file_extra;

		String name = fname;

		if (name.ends_with("/")) {
			// Directory, skip
			ret = unzGoToNextFile(hashing_appx);
			continue;
		}

		name = name.replace("/", "\\");

		block_map->store_string("<File Name=\"" + name
			+ "\" Size=\"" + itos(info.uncompressed_size)
			+ "\" LfhSize=\"" + itos(header_size) + "\">\n"
		);

		Vector<uint8_t> block;
		// 64KB blocks
		block.resize(65536);

		unzOpenCurrentFile(hashing_appx);

		ZPOS64_T stream_start_pos = unzGetCurrentFileZStreamPos64(hashing_appx);
		int bytes_read = unzReadCurrentFile(hashing_appx, block.ptr(), 65536);
		while (bytes_read > 0) {

			uLong offs = unzGetOffset(hashing_appx);
			ZPOS64_T stream_pos = unzGetCurrentFileZStreamPos64(hashing_appx);
			z_off_t tell = unztell(hashing_appx);

			int compressed_size = stream_pos - stream_start_pos;
			stream_start_pos = stream_pos;


			print_line("for " + name + " read " + itos(bytes_read) + " bytes.");
			print_line("    compr: " + itos(compressed_size) + "; offs: " + itos(offs) + "; stream_pos: " + itos(stream_pos) + "; tell: " + itos(tell) + ";");

			sha256_context sha256;
			sha256_init(&sha256);
			sha256_hash(&sha256, block.ptr(), bytes_read);

			char hash[32];
			sha256_done(&sha256, (uint8_t*)hash);

			char base64[60];
			base64_encode(base64, hash, 32);
			base64[59] = '\0';

			String hash_string(base64);

			/*block_map->store_string("\t<Block Hash=\""
				+ hash_string + "\" Size=\""
				+ itos(compressed_size) + "\"/>\n"
			);*/
			block_map->store_string("\t<Block Hash=\""
				+ hash_string + "\"/>\n"
			);

			bytes_read = unzReadCurrentFile(hashing_appx, block.ptr(), 65536);
		}

		block_map->store_string("</File>\n");

		unzCloseCurrentFile(hashing_appx);

		ret = unzGoToNextFile(hashing_appx);
	}

	unzClose(hashing_appx);

	block_map->seek(0);
	Vector<uint8_t> blockmap_buffer;
	blockmap_buffer.resize(block_map->get_len());
	block_map->get_buffer(blockmap_buffer.ptr(), blockmap_buffer.size());
	block_map->close();

	/*zlib_filefunc_def io_hashed = io;
	FileAccess *hashed_f = NULL;
	io_hashed.opaque = &hashed_f;
	zipFile hashed_appx = zipOpen2(unhashed_path.utf8().get_data(), APPEND_STATUS_CREATEAFTER, NULL, &io_hashed);

	zipOpenNewFileInZip(
		hashed_appx,
		"AppxBlockMap.xml",
		NULL,
		NULL,
		0,
		NULL,
		0,
		NULL,
		Z_DEFLATED,
		Z_DEFAULT_COMPRESSION
	);

	zipWriteInFileInZip(hashed_appx, blockmap_buffer.ptr(), blockmap_buffer.size());
	zipCloseFileInZip(hashed_appx);

	zipClose(hashed_appx, NULL);*/

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
