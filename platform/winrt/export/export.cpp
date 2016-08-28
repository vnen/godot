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

/*************************************************************************
 * The code for signing the package was ported from fb-util-for-appx
 * available at https://github.com/facebook/fb-util-for-appx
 * and distributed also under the following license:

BSD License

For fb-util-for-appx software

Copyright (c) 2016, Facebook, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name Facebook nor the names of its contributors may be used to
   endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*************************************************************************/


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
#include <zlib.h>

#ifdef OPENSSL_ENABLED
#include <openssl/bio.h>
#include <openssl/asn1.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs12.h>
#include <openssl/err.h>
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/ossl_typ.h>

namespace asn1 {
	// https://msdn.microsoft.com/en-us/gg463180.aspx

	struct SPCStatementType {
		ASN1_OBJECT *type;
	};
	DECLARE_ASN1_FUNCTIONS(SPCStatementType)

	struct SPCSpOpusInfo {
		ASN1_TYPE *programName;
		ASN1_TYPE *moreInfo;
	};
	DECLARE_ASN1_FUNCTIONS(SPCSpOpusInfo)

	struct DigestInfo {
		X509_ALGOR *digestAlgorithm;
		ASN1_OCTET_STRING *digest;
	};
	DECLARE_ASN1_FUNCTIONS(DigestInfo)

	struct SPCAttributeTypeAndOptionalValue {
		ASN1_OBJECT *type;
		ASN1_TYPE *value;  // SPCInfoValue
	};
	DECLARE_ASN1_FUNCTIONS(SPCAttributeTypeAndOptionalValue)

	// Undocumented.
	struct SPCInfoValue {
		ASN1_INTEGER *i1;
		ASN1_OCTET_STRING *s1;
		ASN1_INTEGER *i2;
		ASN1_INTEGER *i3;
		ASN1_INTEGER *i4;
		ASN1_INTEGER *i5;
		ASN1_INTEGER *i6;
	};
	DECLARE_ASN1_FUNCTIONS(SPCInfoValue)

	struct SPCIndirectDataContent {
		SPCAttributeTypeAndOptionalValue *data;
		DigestInfo *messageDigest;
	};
	DECLARE_ASN1_FUNCTIONS(SPCIndirectDataContent)

	IMPLEMENT_ASN1_FUNCTIONS(SPCIndirectDataContent)
		ASN1_SEQUENCE(SPCIndirectDataContent) = {
		ASN1_SIMPLE(SPCIndirectDataContent, data,
		SPCAttributeTypeAndOptionalValue),
		ASN1_SIMPLE(SPCIndirectDataContent, messageDigest, DigestInfo),
	} ASN1_SEQUENCE_END(SPCIndirectDataContent)

	IMPLEMENT_ASN1_FUNCTIONS(SPCAttributeTypeAndOptionalValue)
		ASN1_SEQUENCE(SPCAttributeTypeAndOptionalValue) = {
		ASN1_SIMPLE(SPCAttributeTypeAndOptionalValue, type,
		ASN1_OBJECT),
		ASN1_OPT(SPCAttributeTypeAndOptionalValue, value, ASN1_ANY),
	} ASN1_SEQUENCE_END(SPCAttributeTypeAndOptionalValue)

	IMPLEMENT_ASN1_FUNCTIONS(SPCInfoValue)
		ASN1_SEQUENCE(SPCInfoValue) = {
		ASN1_SIMPLE(SPCInfoValue, i1, ASN1_INTEGER),
		ASN1_SIMPLE(SPCInfoValue, s1, ASN1_OCTET_STRING),
		ASN1_SIMPLE(SPCInfoValue, i2, ASN1_INTEGER),
		ASN1_SIMPLE(SPCInfoValue, i3, ASN1_INTEGER),
		ASN1_SIMPLE(SPCInfoValue, i4, ASN1_INTEGER),
		ASN1_SIMPLE(SPCInfoValue, i5, ASN1_INTEGER),
		ASN1_SIMPLE(SPCInfoValue, i6, ASN1_INTEGER),
	} ASN1_SEQUENCE_END(SPCInfoValue)

	IMPLEMENT_ASN1_FUNCTIONS(DigestInfo)
		ASN1_SEQUENCE(DigestInfo) = {
		ASN1_SIMPLE(DigestInfo, digestAlgorithm, X509_ALGOR),
		ASN1_SIMPLE(DigestInfo, digest, ASN1_OCTET_STRING),
	} ASN1_SEQUENCE_END(DigestInfo)

	ASN1_SEQUENCE(SPCSpOpusInfo) = {
		ASN1_OPT(SPCSpOpusInfo, programName, ASN1_ANY),
		ASN1_OPT(SPCSpOpusInfo, moreInfo, ASN1_ANY),
	} ASN1_SEQUENCE_END(SPCSpOpusInfo)
	IMPLEMENT_ASN1_FUNCTIONS(SPCSpOpusInfo)

		ASN1_SEQUENCE(SPCStatementType) = {
		ASN1_SIMPLE(SPCStatementType, type, ASN1_OBJECT),
	} ASN1_SEQUENCE_END(SPCStatementType)
	IMPLEMENT_ASN1_FUNCTIONS(SPCStatementType)
}

class EncodedASN1 {

	uint8_t* i_data;
	size_t i_size;

	EncodedASN1(uint8_t** p_data, size_t p_size) {

		i_data = *p_data;
		i_size = p_size;
	}

public:

	template <typename T, int(*TEncode)(T *, uint8_t **)>
	static EncodedASN1 FromItem(T *item) {
		uint8_t *dataRaw = NULL;
		int size = TEncode(item, &dataRaw);

		return EncodedASN1(&dataRaw, size);
	}

	const uint8_t *data() const {
		return i_data;
	}

	size_t size() const {
		return i_size;
	}

	// Assumes the encoded ASN.1 represents a SEQUENCE and puts it into
	// an ASN1_STRING.
	//
	// The returned object holds a copy of this object's data.
	ASN1_STRING* ToSequenceString() {
		ASN1_STRING* string = ASN1_STRING_new();
		if (!string) {
			return NULL;
		}
		if (!ASN1_STRING_set(string, i_data, i_size)) {
			return NULL;
		}
		return string;
	}

	// Assumes the encoded ASN.1 represents a SEQUENCE and puts it into
	// an ASN1_TYPE.
	//
	// The returned object holds a copy of this object's data.
	ASN1_TYPE* ToSequenceType() {
		ASN1_STRING* string = ToSequenceString();
		ASN1_TYPE* type = ASN1_TYPE_new();
		if (!type) {
			return NULL;
		}
		type->type = V_ASN1_SEQUENCE;
		type->value.sequence = string;
		return type;
	}

};

#endif // OPENSSL_ENABLED

class AppxPackager {

	enum {
		FILE_HEADER_MAGIC = 0x04034b50,
		DATA_DESCRIPTOR_MAGIC = 0x08074b50,
		CENTRAL_DIR_MAGIC = 0x02014b50,
		END_OF_CENTRAL_DIR_MAGIC = 0x06054b50,
		ZIP64_END_OF_CENTRAL_DIR_MAGIC = 0x06064b50,
		ZIP64_END_DIR_LOCATOR_MAGIC = 0x07064b50,
		P7X_SIGNATURE= 0x58434b50,
		ZIP64_HEADER_ID = 0x0001,
		ZIP_VERSION = 45,
		GENERAL_PURPOSE = 0x08,
		BASE_FILE_HEADER_SIZE = 30,
		DATA_DESCRIPTOR_SIZE = 24,
		BASE_CENTRAL_DIR_SIZE = 46,
		EXTRA_FIELD_LENGTH = 28,
		ZIP64_HEADER_SIZE = 24,
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
	String tmp_blockmap_file_path;
	String tmp_content_types_file_path;

	Set<String> mime_types;

	Vector<FileMeta> file_metadata;

	ZPOS64_T central_dir_offset;
	ZPOS64_T end_of_central_dir_offset;
	Vector<uint8_t> central_dir_data;

	String hash_block(uint8_t* p_block_data, size_t p_block_len);

	void make_block_map();
	void make_content_types();


	_FORCE_INLINE_ unsigned int AppxPackager::buf_put_int16(uint16_t p_val, uint8_t * p_buf) {
		for (int i = 0; i < 2; i++) {
			*p_buf++ = (p_val >> (i * 8)) & 0xFF;
		}
		return 2;
	}

	_FORCE_INLINE_ unsigned int AppxPackager::buf_put_int32(uint32_t p_val, uint8_t * p_buf) {
		for (int i = 0; i < 4; i++) {
			*p_buf++ = (p_val >> (i * 8)) & 0xFF;
		}
		return 4;
	}

	_FORCE_INLINE_ unsigned int AppxPackager::buf_put_int64(uint64_t p_val, uint8_t * p_buf) {
		for (int i = 0; i < 8; i++) {
			*p_buf++ = (p_val >> (i * 8)) & 0xFF;
		}
		return 8;
	}

	_FORCE_INLINE_ unsigned int AppxPackager::buf_put_string(String p_val, uint8_t * p_buf) {
		for (int i = 0; i < p_val.length(); i++) {
			*p_buf++ = p_val.utf8().get(i);
		}
		return p_val.length();
	}

	int write_file_header(String p_name, bool p_compress, bool p_do_hash = true);
	int write_file_descriptor(uint32_t p_crc32, size_t p_compressed_size, size_t p_uncompressed_size, bool p_do_hash = true);
	void store_central_dir_header(const FileMeta p_file, bool p_do_hash = true);
	void write_zip64_end_of_central_record();
	void write_end_of_central_record();

	String content_type(String p_extension);

#ifdef OPENSSL_ENABLED

	// Signing methods and structs:

	String certificate_path;
	String certificate_pass;
	bool sign_package;

	struct CertFile {

		EVP_PKEY* private_key;
		X509* certificate;
	};

	SHA256_CTX axpc_context; // SHA256 context for ZIP file entries
	SHA256_CTX axcd_context; // SHA256 context for ZIP directory entries

	struct AppxDigests {

		uint8_t axpc[SHA256_DIGEST_LENGTH]; // ZIP file entries
		uint8_t axcd[SHA256_DIGEST_LENGTH]; // ZIP directory entry
		uint8_t axct[SHA256_DIGEST_LENGTH]; // Content types XML
		uint8_t axbm[SHA256_DIGEST_LENGTH]; // Block map XML
		uint8_t axci[SHA256_DIGEST_LENGTH]; // Code Integrity file (optional)
	};

	CertFile cert_file;
	AppxDigests digests;

	void MakeSPCInfoValue(asn1::SPCInfoValue &info);
	Error MakeIndirectDataContent(asn1::SPCIndirectDataContent &idc);
	Error add_attributes(PKCS7_SIGNER_INFO *signerInfo);
	void make_digests();
	void write_digest(Vector<uint8_t> &p_out_buffer);

	Error openssl_error(unsigned long p_err);
	Error read_cert_file(const String &p_path, const String &p_password, CertFile* p_out_cf);
	Error sign(const CertFile &p_cert, const AppxDigests &digests, PKCS7* p_out_signature);

#endif // OPENSSL_ENABLED

public:

	enum SignOption {

		SIGN,
		DONT_SIGN,
	};

	void init(FileAccess* p_fa, EditorProgress* p_progress, SignOption p_sign, String &p_certificate_path, String &p_certificate_password);
	void add_file(String p_file_name, const uint8_t* p_buffer, size_t p_len, int p_file_no, int p_total_files, bool p_compress = false);
	void finish();

	AppxPackager();
	~AppxPackager();
};

class EditorExportPlatformWinrt : public EditorExportPlatform {

	OBJ_TYPE(EditorExportPlatformWinrt, EditorExportPlatform);

	Ref<ImageTexture> logo;

	bool export_x86;
	bool export_x64;
	bool export_arm;

	bool is_debug;

	String custom_release_package;
	String custom_debug_package;

	bool sign_package;
	String certificate_path;
	String certificate_pass;

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


///////////////////////////////////////////////////////////////////////////

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

	FileAccess* tmp_file = FileAccess::open(tmp_blockmap_file_path, FileAccess::WRITE);

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

	FileAccess* tmp_file = FileAccess::open(tmp_content_types_file_path, FileAccess::WRITE);

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

	// Appx signature file
	tmp_file->store_string("<Default Extension=\"p7x\" ContentType=\"application/octet-stream\" />");

	// Override for package files
	tmp_file->store_string("<Override PartName=\"/AppxManifest.xml\" ContentType=\"application/vnd.ms-appx.manifest+xml\" />");
	tmp_file->store_string("<Override PartName=\"/AppxBlockMap.xml\" ContentType=\"application/vnd.ms-appx.blockmap+xml\" />");
	tmp_file->store_string("<Override PartName=\"/AppxSignature.p7x\" ContentType=\"application/vnd.ms-appx.signature\" />");
	tmp_file->store_string("<Override PartName=\"/AppxMetadata/CodeIntegrity.cat\" ContentType=\"application/vnd.ms-pkiseccat\" />");

	tmp_file->store_string("</Types>");

	tmp_file->close();
	memdelete(tmp_file);
	tmp_file = NULL;
}

int AppxPackager::write_file_header(String p_name, bool p_compress, bool p_do_hash) {

	Vector<uint8_t> buf;
	buf.resize(BASE_FILE_HEADER_SIZE + p_name.length());

	int offs = 0;
	// Write magic
	offs += buf_put_int32(FILE_HEADER_MAGIC, &buf[offs]);

	// Version
	offs += buf_put_int16(ZIP_VERSION, &buf[offs]);

	// Special flag
	offs += buf_put_int16(GENERAL_PURPOSE, &buf[offs]);

	// Compression
	offs += buf_put_int16(p_compress ? Z_DEFLATED : 0, &buf[offs]);

	// Empty header data
	for (int i = 0; i < 16; i++) {
		buf[offs++] = 0;
	}

	// File name length
	offs += buf_put_int16(p_name.length(), &buf[offs]);

	// Extra data length
	offs += buf_put_int16(0, &buf[offs]);

	// File name
	offs += buf_put_string(p_name, &buf[offs]);

#ifdef OPENSSL_ENABLED
	// Calculate the hash for signing
	if (p_do_hash)
		SHA256_Update(&axpc_context, buf.ptr(), buf.size());
#endif // OPENSSL_ENABLED

	// Done!
	package->store_buffer(buf.ptr(), buf.size());

	return buf.size();
}

int AppxPackager::write_file_descriptor(uint32_t p_crc32, size_t p_compressed_size, size_t p_uncompressed_size, bool p_do_hash) {

	Vector<uint8_t> buf;
	buf.resize(DATA_DESCRIPTOR_SIZE);

	int offs = 0;

	// Write magic
	offs += buf_put_int32(DATA_DESCRIPTOR_MAGIC, &buf[offs]);

	// CRC
	offs += buf_put_int32(p_crc32, &buf[offs]);

	// Compressed size
	offs += buf_put_int64(p_compressed_size, &buf[offs]);

	// Uncompressed size
	offs += buf_put_int64(p_uncompressed_size, &buf[offs]);

#ifdef OPENSSL_ENABLED
	// Calculate the hash for signing
	if (p_do_hash)
		SHA256_Update(&axpc_context, buf.ptr(), buf.size());
#endif // OPENSSL_ENABLED

	// Done!
	package->store_buffer(buf.ptr(), buf.size());

	return buf.size();
}

void AppxPackager::store_central_dir_header(const FileMeta p_file, bool p_do_hash) {

	Vector<uint8_t> &buf = central_dir_data;
	int offs = buf.size();
	buf.resize(buf.size() + BASE_CENTRAL_DIR_SIZE + p_file.name.length() + EXTRA_FIELD_LENGTH);


	// Write magic
	offs += buf_put_int32(CENTRAL_DIR_MAGIC, &buf[offs]);

	// Version (twice)
	offs += buf_put_int16(ZIP_VERSION, &buf[offs]);
	offs += buf_put_int16(ZIP_VERSION, &buf[offs]);

	// General purpose flag
	offs += buf_put_int16(GENERAL_PURPOSE, &buf[offs]);

	// Compression
	offs += buf_put_int16(p_file.compressed ? Z_DEFLATED : 0, &buf[offs]);

	// Modification date/time
	offs += buf_put_int32(0, &buf[offs]);

	// Crc-32
	offs += buf_put_int32(p_file.file_crc32, &buf[offs]);

	// File sizes (will be in extra field)
	for (int i = 0; i < 8; i++) {
		buf[offs++] = 0xFF;
	}

	// File name length
	offs += buf_put_int16(p_file.name.length(), &buf[offs]);

	// Extra field length
	offs += buf_put_int16(EXTRA_FIELD_LENGTH, &buf[offs]);

	// Comment length
	offs += buf_put_int16(0, &buf[offs]);

	// Disk number start, internal/external file attributes
	for (int i = 0; i < 8; i++) {
		buf[offs++] = 0;
	}

	// Relative offset (will be on extra field)
	for (int i = 0; i < 4; i++) {
		buf[offs++] = 0xFF;
	}

	// File name
	offs += buf_put_string(p_file.name, &buf[offs]);

	// Zip64 extra field
	offs += buf_put_int16(ZIP64_HEADER_ID, &buf[offs]);
	offs += buf_put_int16(ZIP64_HEADER_SIZE, &buf[offs]);

	// Original size
	offs += buf_put_int64(p_file.uncompressed_size, &buf[offs]);

	// Compressed size
	offs += buf_put_int64(p_file.compressed_size, &buf[offs]);

	// File offset
	offs += buf_put_int64(p_file.zip_offset, &buf[offs]);

#ifdef OPENSSL_ENABLED
	// Calculate the hash for signing
	if (p_do_hash)
		SHA256_Update(&axcd_context, buf.ptr(), buf.size());
#endif // OPENSSL_ENABLED

	// Done!
}

void AppxPackager::write_zip64_end_of_central_record() {

	Vector<uint8_t> buf;
	buf.resize(ZIP64_END_OF_CENTRAL_DIR_SIZE + 12); // Size plus magic

	int offs = 0;

	// Write magic
	offs += buf_put_int32(ZIP64_END_OF_CENTRAL_DIR_MAGIC, &buf[offs]);

	// Size of this record
	offs += buf_put_int64(ZIP64_END_OF_CENTRAL_DIR_SIZE, &buf[offs]);

	// Version (yes, twice)
	offs += buf_put_int16(ZIP_VERSION, &buf[offs]);
	offs += buf_put_int16(ZIP_VERSION, &buf[offs]);

	// Disk number
	for (int i = 0; i < 8; i++) {
		buf[offs++] = 0;
	}

	// Number of entries (total and per disk)
	offs += buf_put_int64(file_metadata.size(), &buf[offs]);
	offs += buf_put_int64(file_metadata.size(), &buf[offs]);

	// Size of central dir
	offs += buf_put_int64(central_dir_data.size(), &buf[offs]);

	// Central dir offset
	offs += buf_put_int64(central_dir_offset, &buf[offs]);

	// Done!
	package->store_buffer(buf.ptr(), buf.size());
}

void AppxPackager::write_end_of_central_record() {

	Vector<uint8_t> buf;
	buf.resize(END_OF_CENTRAL_DIR_SIZE);

	int offs = 0;

	// Write magic for zip64 central dir locator
	offs += buf_put_int32(ZIP64_END_DIR_LOCATOR_MAGIC, &buf[offs]);

	// Disk number
	for (int i = 0; i < 4; i++) {
		buf[offs++] = 0;
	}

	// Relative offset
	offs += buf_put_int64(end_of_central_dir_offset, &buf[offs]);

	// Number of disks
	offs += buf_put_int32(1, &buf[offs]);

	// Write magic for end central dir
	offs += buf_put_int32(END_OF_CENTRAL_DIR_MAGIC, &buf[offs]);

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

void AppxPackager::init(FileAccess * p_fa, EditorProgress* p_progress, SignOption p_sign, String &p_certificate_path, String &p_certificate_password) {

	progress = p_progress;
	package = p_fa;
	central_dir_offset = 0;
	end_of_central_dir_offset = 0;
	tmp_blockmap_file_path = EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmpblockmap.xml";
	tmp_content_types_file_path = EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmpcontenttypes.xml";
#ifdef OPENSSL_ENABLED
	certificate_path = p_certificate_path;
	certificate_pass = p_certificate_password;
	sign_package = p_sign == SignOption::SIGN;
	SHA256_Init(&axpc_context);
	SHA256_Init(&axcd_context);
#endif // OPENSSL_ENABLED
}

void AppxPackager::add_file(String p_file_name, const uint8_t * p_buffer, size_t p_len, int p_file_no, int p_total_files, bool p_compress) {

	if (p_file_no >= 1 && p_total_files >= 1) {
		progress->step("File: " + p_file_name, 3 + p_file_no * 100 / p_total_files);
	}

	bool do_hash = p_file_name != "AppxSignature.p7x";

	FileMeta meta;
	meta.name = p_file_name;
	meta.uncompressed_size = p_len;
	meta.compressed_size = p_len;
	meta.compressed = p_compress;
	meta.zip_offset = package->get_pos();


	// Create file header
	meta.lfh_size = write_file_header(p_file_name, p_compress, do_hash);

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
#ifdef OPENSSL_ENABLED
			if (do_hash)
				SHA256_Update(&axpc_context, strm_out.ptr(), strm.total_out - total_out_before);
#endif // OPENSSL_ENABLED

		} else {
			bh.compressed_size = block_size;
			package->store_buffer(strm_in.ptr(), block_size);
#ifdef OPENSSL_ENABLED
			if (do_hash)
				SHA256_Update(&axpc_context, strm_in.ptr(), block_size);
#endif // OPENSSL_ENABLED
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
#ifdef OPENSSL_ENABLED
		if (do_hash)
			SHA256_Update(&axpc_context, strm_out.ptr(), strm.total_out - total_out_before);
#endif // OPENSSL_ENABLED

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
	write_file_descriptor(crc, meta.compressed_size, meta.uncompressed_size, do_hash);

	file_metadata.push_back(meta);
}

void AppxPackager::finish() {

	// Create and add block map file
	progress->step("Creating block map...", 103);
	make_block_map();
	FileAccess* blockmap_file = FileAccess::open(tmp_blockmap_file_path, FileAccess::READ);
	Vector<uint8_t> blockmap_buffer;
	blockmap_buffer.resize(blockmap_file->get_len());

	blockmap_file->get_buffer(blockmap_buffer.ptr(), blockmap_buffer.size());

#ifdef OPENSSL_ENABLED
	// Hash the file for signing
	if (sign_package) {
		SHA256_CTX axbm_context;
		SHA256_Init(&axbm_context);
		SHA256_Update(&axbm_context, blockmap_buffer.ptr(), blockmap_buffer.size());
		SHA256_Final(digests.axbm, &axbm_context);
	}
#endif // OPENSSL_ENABLED

	add_file("AppxBlockMap.xml", blockmap_buffer.ptr(), blockmap_buffer.size(), -1, -1, true);

	blockmap_file->close();
	memdelete(blockmap_file);
	blockmap_file = NULL;

	// Add content types
	progress->step("Setting content types...", 104);
	make_content_types();

	FileAccess* types_file = FileAccess::open(tmp_content_types_file_path, FileAccess::READ);
	Vector<uint8_t> types_buffer;
	types_buffer.resize(types_file->get_len());

	types_file->get_buffer(types_buffer.ptr(), types_buffer.size());

#ifdef OPENSSL_ENABLED
	if (sign_package) {
		// Hash the file for signing
		SHA256_CTX axct_context;
		SHA256_Init(&axct_context);
		SHA256_Update(&axct_context, types_buffer.ptr(), types_buffer.size());
		SHA256_Final(digests.axct, &axct_context);
	}
#endif // OPENSSL_ENABLED

	add_file("[Content_Types].xml", types_buffer.ptr(), types_buffer.size(), -1, -1, true);

	types_file->close();
	memdelete(types_file);
	types_file = NULL;

	// Pre-process central directory before signing
	for (int i = 0; i < file_metadata.size(); i++) {
		store_central_dir_header(file_metadata[i]);
	}

#ifdef OPENSSL_ENABLED
	// Create the signature file
	if (sign_package) {

		Error err = read_cert_file(certificate_path, certificate_pass, &cert_file);

		if (err != OK) {
			EditorNode::add_io_error(TTR("Couldn't read the certficate file. Are the path and password both correct?"));
			package->close();
			memdelete(package);
			package = NULL;
			return;
		}

		make_digests();

		PKCS7* signature = PKCS7_new();
		if (!signature) {
			EditorNode::add_io_error(TTR("Error creating the signature object."));
			package->close();
			memdelete(package);
			package = NULL;
			return;
		}

		err = sign(cert_file, digests, signature);

		if (err != OK) {
			EditorNode::add_io_error(TTR("Error creating the package signature."));
			package->close();
			memdelete(package);
			package = NULL;
			return;
		}

		// Read the signature as bytes
		BIO* bio_out = BIO_new(BIO_s_mem());
		i2d_PKCS7_bio(bio_out, signature);

		BIO_flush(bio_out);

		uint8_t* bio_ptr;
		size_t bio_size = BIO_get_mem_data(bio_out, &bio_ptr);

		// Create the signature buffer with magic number
		Vector<uint8_t> signature_file;
		signature_file.resize(4 + bio_size);
		buf_put_int32(P7X_SIGNATURE, signature_file.ptr());
		for (int i = 0; i < bio_size; i++)
			signature_file[i + 4] = bio_ptr[i];

		// Add the signature to the package
		add_file("AppxSignature.p7x", signature_file.ptr(), signature_file.size(), -1, -1, true);

		// Add central directory entry
		store_central_dir_header(file_metadata[file_metadata.size() - 1], false);
	}
#endif // OPENSSL_ENABLED


	// Write central directory
	progress->step("Finishing package...", 105);
	central_dir_offset = package->get_pos();
	package->store_buffer(central_dir_data.ptr(), central_dir_data.size());

	// End record
	end_of_central_dir_offset = package->get_pos();
	write_zip64_end_of_central_record();
	write_end_of_central_record();

	package->close();
	memdelete(package);
	package = NULL;
}

#ifdef OPENSSL_ENABLED
// https://support.microsoft.com/en-us/kb/287547
const char SPC_INDIRECT_DATA_OBJID[] = "1.3.6.1.4.1.311.2.1.4";
const char SPC_STATEMENT_TYPE_OBJID[] = "1.3.6.1.4.1.311.2.1.11";
const char SPC_SP_OPUS_INFO_OBJID[] = "1.3.6.1.4.1.311.2.1.12";
const char SPC_SIPINFO_OBJID[] = "1.3.6.1.4.1.311.2.1.30";
#endif // OPENSSL_ENABLED

AppxPackager::AppxPackager() {}

AppxPackager::~AppxPackager() {}


////////////////////////////////////////////////////////////////////

#ifdef OPENSSL_ENABLED
Error AppxPackager::openssl_error(unsigned long p_err) {

	ERR_load_crypto_strings();

	char buffer[256];
	ERR_error_string_n(p_err, buffer, sizeof(buffer));

	String err(buffer);

	ERR_EXPLAIN(err);
	ERR_FAIL_V(FAILED);
}

void AppxPackager::MakeSPCInfoValue(asn1::SPCInfoValue &info) {

	// I have no idea what these numbers mean.
	static uint8_t s1Magic[] = {
		0x4B, 0xDF, 0xC5, 0x0A, 0x07, 0xCE, 0xE2, 0x4D,
		0xB7, 0x6E, 0x23, 0xC8, 0x39, 0xA0, 0x9F, 0xD1,
	};
	ASN1_INTEGER_set(info.i1, 0x01010000);
	ASN1_OCTET_STRING_set(info.s1, s1Magic, sizeof(s1Magic));
	ASN1_INTEGER_set(info.i2, 0x00000000);
	ASN1_INTEGER_set(info.i3, 0x00000000);
	ASN1_INTEGER_set(info.i4, 0x00000000);
	ASN1_INTEGER_set(info.i5, 0x00000000);
	ASN1_INTEGER_set(info.i6, 0x00000000);
}

Error AppxPackager::MakeIndirectDataContent(asn1::SPCIndirectDataContent &idc) {

	using namespace asn1;

	ASN1_TYPE* algorithmParameter = ASN1_TYPE_new();
	if (!algorithmParameter) {
		return openssl_error(ERR_peek_last_error());
	}
	algorithmParameter->type = V_ASN1_NULL;

	SPCInfoValue* infoValue = SPCInfoValue_new();
	if (!infoValue) {
		return openssl_error(ERR_peek_last_error());
	}
	MakeSPCInfoValue(*infoValue);

	ASN1_TYPE* value =
		EncodedASN1::FromItem<asn1::SPCInfoValue,
		asn1::i2d_SPCInfoValue>(infoValue)
		.ToSequenceType();

	{
		Vector<uint8_t> digest;
		write_digest(digest);
		if (!ASN1_OCTET_STRING_set(idc.messageDigest->digest,
			digest.ptr(), digest.size())) {

			return openssl_error(ERR_peek_last_error());
		}
	}

	idc.data->type = OBJ_txt2obj(SPC_SIPINFO_OBJID, 1);
	idc.data->value = value;
	idc.messageDigest->digestAlgorithm->algorithm = OBJ_nid2obj(NID_sha256);
	idc.messageDigest->digestAlgorithm->parameter = algorithmParameter;
}

Error AppxPackager::add_attributes(PKCS7_SIGNER_INFO * p_signer_info) {

	// Add opus attribute
	asn1::SPCSpOpusInfo* opus = asn1::SPCSpOpusInfo_new();
	if (!opus) return openssl_error(ERR_peek_last_error());

	ASN1_STRING* opus_value = EncodedASN1::FromItem<asn1::SPCSpOpusInfo, asn1::i2d_SPCSpOpusInfo>(opus)
		.ToSequenceString();

	if (!PKCS7_add_signed_attribute(
		p_signer_info,
		OBJ_txt2nid(SPC_SP_OPUS_INFO_OBJID),
		V_ASN1_SEQUENCE,
		opus_value
	)) {

		asn1::SPCSpOpusInfo_free(opus);

		ASN1_STRING_free(opus_value);
		return openssl_error(ERR_peek_last_error());
	}

	// Add content type attribute
	if (!PKCS7_add_signed_attribute(
		p_signer_info,
		NID_pkcs9_contentType,
		V_ASN1_OBJECT,
		OBJ_txt2obj(SPC_INDIRECT_DATA_OBJID, 1)
	)) {

		asn1::SPCSpOpusInfo_free(opus);
		ASN1_STRING_free(opus_value);
		return openssl_error(ERR_peek_last_error());
	}

	// Add statement type attribute
	asn1::SPCStatementType* statement_type = asn1::SPCStatementType_new();
	if (!statement_type) return openssl_error(ERR_peek_last_error());

	statement_type->type = OBJ_nid2obj(NID_ms_code_ind);
	ASN1_STRING* statement_type_value =
		EncodedASN1::FromItem<asn1::SPCStatementType, asn1::i2d_SPCStatementType>(statement_type)
		.ToSequenceString();

	if (!PKCS7_add_signed_attribute(
		p_signer_info,
		OBJ_txt2nid(SPC_STATEMENT_TYPE_OBJID),
		V_ASN1_SEQUENCE,
		statement_type_value
	)) {

		ASN1_STRING_free(opus_value);
		asn1::SPCStatementType_free(statement_type);
		ASN1_STRING_free(statement_type_value);

		return openssl_error(ERR_peek_last_error());
	}

}

void AppxPackager::make_digests() {

	// AXPC
	SHA256_Final(digests.axpc, &axpc_context);

	// AXCD
	SHA256_Final(digests.axcd, &axcd_context);

	// AXCI
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
		digests.axci[i] = 0;

}

void AppxPackager::write_digest(Vector<uint8_t>& p_out_buffer) {

	// Size of digests plus 6 32-bit magic numbers
	p_out_buffer.resize((SHA256_DIGEST_LENGTH * 5) + (6 * 4));

	int offs = 0;

	// APPX
	uint32_t sig = 0x58505041;
	offs += buf_put_int32(sig, &p_out_buffer[offs]);

	// AXPC
	uint32_t axpc_sig = 0x43505841;
	offs += buf_put_int32(axpc_sig, &p_out_buffer[offs]);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		p_out_buffer[offs++] = digests.axpc[i];
	}

	// AXCD
	uint32_t axcd_sig = 0x44435841;
	offs += buf_put_int32(axcd_sig, &p_out_buffer[offs]);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		p_out_buffer[offs++] = digests.axcd[i];
	}

	// AXCT
	uint32_t axct_sig = 0x54435841;
	offs += buf_put_int32(axct_sig, &p_out_buffer[offs]);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		p_out_buffer[offs++] = digests.axct[i];
	}

	// AXBM
	uint32_t axbm_sig = 0x4D425841;
	offs += buf_put_int32(axbm_sig, &p_out_buffer[offs]);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		p_out_buffer[offs++] = digests.axbm[i];
	}

	// AXCI
	uint32_t axci_sig = 0x49435841;
	offs += buf_put_int32(axci_sig, &p_out_buffer[offs]);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		p_out_buffer[offs++] = digests.axci[i];
	}

	// Done!
}

Error AppxPackager::read_cert_file(const String & p_path, const String &p_password, CertFile* p_out_cf) {

	ERR_FAIL_COND_V(!p_out_cf, ERR_INVALID_PARAMETER);

	BIO* bio = BIO_new_file(p_path.utf8().get_data(), "rb");
	if (!bio) {
		return openssl_error(ERR_peek_last_error());
	}

	PKCS12* data = d2i_PKCS12_bio(bio, NULL);
	if (!data) {
		BIO_free(bio);
		return openssl_error(ERR_peek_last_error());
	}

	if (!PKCS12_parse(data, p_password.utf8().get_data(), &p_out_cf->private_key, &p_out_cf->certificate, NULL)) {
		PKCS12_free(data);
		BIO_free(bio);
		return openssl_error(ERR_peek_last_error());
	}

	if (!p_out_cf->private_key) {
		PKCS12_free(data);
		BIO_free(bio);
		return openssl_error(ERR_peek_last_error());
	}

	if (!p_out_cf->certificate) {
		PKCS12_free(data);
		BIO_free(bio);
		return openssl_error(ERR_peek_last_error());
	}

	PKCS12_free(data);
	BIO_free(bio);

	return OK;
}

Error AppxPackager::sign(const CertFile & p_cert, const AppxDigests & digests, PKCS7 * p_out_signature) {

	OpenSSL_add_all_algorithms();

	// Register object IDs
	OBJ_create_and_add_object(SPC_INDIRECT_DATA_OBJID, NULL, NULL);
	OBJ_create_and_add_object(SPC_SIPINFO_OBJID, NULL, NULL);
	OBJ_create_and_add_object(SPC_SP_OPUS_INFO_OBJID, NULL, NULL);
	OBJ_create_and_add_object(SPC_STATEMENT_TYPE_OBJID, NULL, NULL);

	if (!PKCS7_set_type(p_out_signature, NID_pkcs7_signed)) {

		return openssl_error(ERR_peek_last_error());
	}

	PKCS7_SIGNER_INFO *signer_info = PKCS7_add_signature(p_out_signature, p_cert.certificate, p_cert.private_key, EVP_sha256());
	if (!signer_info) return openssl_error(ERR_peek_last_error());

	add_attributes(signer_info);

	if (!PKCS7_content_new(p_out_signature, NID_pkcs7_data)) {

		return openssl_error(ERR_peek_last_error());
	}

	if (!PKCS7_add_certificate(p_out_signature, p_cert.certificate)) {

		return openssl_error(ERR_peek_last_error());
	}

	asn1::SPCIndirectDataContent* idc = asn1::SPCIndirectDataContent_new();

	MakeIndirectDataContent(*idc);
	EncodedASN1 idc_encoded =
		EncodedASN1::FromItem<asn1::SPCIndirectDataContent, asn1::i2d_SPCIndirectDataContent>(idc);

	BIO* signed_data = PKCS7_dataInit(p_out_signature, NULL);

	if (idc_encoded.size() < 2) {

		ERR_EXPLAIN("Invalid encoded size");
		ERR_FAIL_V(FAILED);
	}

	if ((idc_encoded.data()[1] & 0x80) == 0x00) {

		ERR_EXPLAIN("Invalid encoded data");
		ERR_FAIL_V(FAILED);
	}

	size_t skip = 4;

	if (BIO_write(signed_data, idc_encoded.data() + skip, idc_encoded.size() - skip)
		!= idc_encoded.size() - skip) {

		return openssl_error(ERR_peek_last_error());
	}
	if (BIO_flush(signed_data) != 1) {

		return openssl_error(ERR_peek_last_error());
	}

	if (!PKCS7_dataFinal(p_out_signature, signed_data)) {

		return openssl_error(ERR_peek_last_error());
	}

	PKCS7* content = PKCS7_new();
	if (!content) {

		return openssl_error(ERR_peek_last_error());
	}

	content->type = OBJ_txt2obj(SPC_INDIRECT_DATA_OBJID, 1);

	ASN1_TYPE* idc_sequence = idc_encoded.ToSequenceType();
	content->d.other = idc_sequence;

	if (!PKCS7_set_content(p_out_signature, content)) {

		return openssl_error(ERR_peek_last_error());
	}

	return OK;
}

#endif // OPENSSL_ENABLED

////////////////////////////////////////////////////////////////////


Error EditorExportPlatformWinrt::save_appx_file(void * p_userdata, const String & p_path, const Vector<uint8_t>& p_data, int p_file, int p_total) {

	AppxPackager *packager = (AppxPackager*)p_userdata;
	String dst_path = p_path.replace_first("res://", "game/");

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
	else if (n == "signing/sign")
		sign_package = p_value;
	else if (n == "signing/certificate_file")
		certificate_path = p_value;
	else if (n == "signing/certificate_password")
		certificate_pass = p_value;
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
	else if (n == "signing/sign")
		r_ret = sign_package;
	else if (n == "signing/certificate_file")
		r_ret = certificate_path;
	else if (n == "signing/certificate_password")
		r_ret = certificate_pass;
	else return false;

	return true;
}

void EditorExportPlatformWinrt::_get_property_list(List<PropertyInfo>* p_list) const {

	p_list->push_back(PropertyInfo(Variant::STRING, "custom_package/debug", PROPERTY_HINT_GLOBAL_FILE, "appx"));
	p_list->push_back(PropertyInfo(Variant::STRING, "custom_package/release", PROPERTY_HINT_GLOBAL_FILE, "appx"));

	p_list->push_back(PropertyInfo(Variant::BOOL, "architecture/arm"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "architecture/x86"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "architecture/x64"));

#if 0 // Signing does not work :( disabling for now
	p_list->push_back(PropertyInfo(Variant::BOOL, "signing/sign"));
	p_list->push_back(PropertyInfo(Variant::STRING, "signing/certificate_file", PROPERTY_HINT_GLOBAL_FILE, "pfx"));
	p_list->push_back(PropertyInfo(Variant::STRING, "signing/certificate_password"));
#endif
}

bool EditorExportPlatformWinrt::can_export(String * r_error) const {

	String err;
	bool valid = true;

	if (!exists_export_template("winrt_x86_debug.zip") || !exists_export_template("winrt_x86_release.zip")) {
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
			src_appx = find_export_template("winrt_x86_debug.zip", &err);
		} else {
			src_appx = find_export_template("winrt_x86_release.zip", &err);
		}
		if (src_appx == "") {
			EditorNode::add_io_error(err);
			return ERR_FILE_NOT_FOUND;
		}
	}

	Error err = OK;

	//CertFile cf;
	//err = read_cert_file(certificate_path, certificate_pass, &cf);
	//if (err != OK) return err;

	FileAccess *fa_pack = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	AppxPackager packager;
	packager.init(fa_pack, &ep, sign_package ? AppxPackager::SIGN : AppxPackager::DONT_SIGN, certificate_path, certificate_pass);

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

	sign_package = false;
}

EditorExportPlatformWinrt::~EditorExportPlatformWinrt() {}


void register_winrt_exporter() {

	Ref<EditorExportPlatformWinrt> exporter = Ref<EditorExportPlatformWinrt>(memnew(EditorExportPlatformWinrt));
	EditorImportExport::get_singleton()->add_export_platform(exporter);
}
