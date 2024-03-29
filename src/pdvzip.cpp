// 	PNG Data Vehicle, ZIP Edition (PDVZIP v1.8). Created by Nicholas Cleasby (@CleasbyCode) 6/08/2022

//	To compile program (Linux):
// 	$ g++ pdvzip.cpp -O2 -s -o pdvzip

// 	Run it:
// 	$ ./pdvzip

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

typedef unsigned char Byte;

struct PDV_STRUCT {
	const size_t MAX_FILE_SIZE = 209715200;
	std::vector<Byte> Image_Vec, Zip_Vec, Script_Vec;
	const std::string BAD_CHAR = "\x22\x27\x28\x29\x3B\x3E\x60";
	std::string image_name, zip_name;
	size_t image_size{}, zip_size{}, script_size{}, combined_file_size{};
	bool big_endian = true;
};

size_t
	// Code to compute CRC32 (for "IDAT" & "iCCP" chunks within this program) is taken from: https://www.w3.org/TR/2003/REC-PNG-20031110/#D-CRCAppendix 
	Crc_Update(const size_t&, Byte*, const size_t&),
	Crc(Byte*, const size_t&);

void
	// Attempt to open PNG & ZIP file, followed by some initial file size checks. Display relevant error message and exit program if any file fails to open or fails size checks.
	Open_Files(PDV_STRUCT&),
	// Various image file checks to make sure image is valid and meets program's requirements. Display relevant error message if checks fail, exit program.
	Check_Image_File(PDV_STRUCT&, std::ifstream&, std::ifstream&),
	// Various ZIP file checks to make sure archive is valid and meets program's requirements. Display relevant error message if checks fail, exit program.
	Check_Zip_File(PDV_STRUCT&, std::ifstream&),
	// Keep critical PNG chunks, remove the rest.
	Erase_Image_Chunks(PDV_STRUCT&, std::ifstream&),
	// Update barebones extraction script determined by embedded ZIP file content. 
	Complete_Extraction_Script(PDV_STRUCT&),
	// Insert contents of vectors storing user ZIP file and the completed extraction script into the vector containing PNG image. This is our PNG-ZIP polyglot.
	Combine_Vectors(PDV_STRUCT&),
	// Adjust embedded ZIP file offsets within the PNG image to their new index locations, so that it remains a valid, working ZIP archive. 
	Fix_Zip_Offset(PDV_STRUCT&, const size_t&),
	// Write out to file the complete ZIP embedded PNG image file, creating our PNG-ZIP polyglot.
	Write_Out_Polyglot_File(PDV_STRUCT&),
	// Update values, such as chunk lengths, CRC, file sizes and other values. Writes them into the relevant vector index locations.
	Value_Updater(std::vector<Byte>&, size_t, const size_t&, int, bool),
	// Output to screen detailed program usage information.
	Display_Info();

int main(int argc, char** argv) {

	PDV_STRUCT pdv;

	if (argc == 2 && std::string(argv[1]) == "--info") {
		Display_Info();
	}
	else if (argc < 3 || argc > 3) {
		std::cout << "\nUsage: pdvzip <cover_image> <zip_file>\n\t\bpdvzip --info\n\n";
	}
	else {
		pdv.image_name = argv[1];
		pdv.zip_name = argv[2];

		const std::regex REG_EXP("(\\.[a-zA-Z_0-9\\.\\\\\\s\\-\\/]+)?[a-zA-Z_0-9\\.\\\\\\s\\-\\/]+?(\\.[a-zA-Z0-9]+)?");

		const std::string
			// Get file extensions from image and data file names.
			GET_PNG_EXT = pdv.image_name.length() > 2 ? pdv.image_name.substr(pdv.image_name.length() - 3) : pdv.image_name,
			GET_ZIP_EXT = pdv.zip_name.length() > 2 ? pdv.zip_name.substr(pdv.zip_name.length() - 3) : pdv.zip_name;

		if (GET_PNG_EXT != "png" || GET_ZIP_EXT != "zip" || !regex_match(pdv.image_name, REG_EXP) || !regex_match(pdv.zip_name, REG_EXP)) {
			// Either file contains an incorrect file extension and/or invalid input. Display error message and exit program.
			std::cerr << (GET_PNG_EXT != "png" || GET_ZIP_EXT != "zip" ? "\nFile Type Error: Invalid file extension found. Only expecting 'png' followed by 'zip'"
				: "\nInvalid Input Error: Characters not supported by this program found within file name arguments") << ".\n\n";
			std::exit(EXIT_FAILURE);
		}
		Open_Files(pdv);
	}
	return 0;
}

void Open_Files(PDV_STRUCT& pdv) {

	std::cout << "\nReading files. Please wait...\n";

	// Attempt to open user's files.
	std::ifstream
		image_ifs(pdv.image_name, std::ios::binary),
		zip_ifs(pdv.zip_name, std::ios::binary);

	if (!image_ifs || !zip_ifs) {
		// Display relevant error message and exit program if any file fails to open.
		std::cerr << "\nRead File Error: " << (!image_ifs ? "Unable to open image file" : "Unable to open ZIP file") << ".\n\n";
		std::exit(EXIT_FAILURE);
	}
	else {
		// Initial file size checks. We will need to check sizes again, later in the program.
		constexpr int
			MIN_IMAGE_SIZE = 68,
			MIN_ZIP_SIZE = 40;

		bool file_size_check = false;

		// Get PNG file size.
		image_ifs.seekg(0, image_ifs.end);
		pdv.image_size = image_ifs.tellg();
		image_ifs.seekg(0, image_ifs.beg);

		// Get ZIP file size
		zip_ifs.seekg(0, zip_ifs.end);
		pdv.zip_size = zip_ifs.tellg();
		zip_ifs.seekg(0, zip_ifs.beg);

		pdv.combined_file_size = pdv.image_size + pdv.zip_size;

		file_size_check = pdv.image_size > MIN_IMAGE_SIZE && pdv.zip_size > MIN_ZIP_SIZE && pdv.MAX_FILE_SIZE >= pdv.combined_file_size;

		if (!file_size_check) {
			// Display relevant error message and exit program if any size check fails.
			std::cerr << "\nFile Size Error: " << (MIN_IMAGE_SIZE > pdv.image_size ? "Invalid PNG image. File too small"
				: (MIN_ZIP_SIZE > pdv.zip_size ? "Invalid ZIP file. File too small"
					: "The combined file size of your PNG image and ZIP file exceeds maximum limit")) << ".\n\n";
			std::exit(EXIT_FAILURE);
		}
		Check_Image_File(pdv, image_ifs, zip_ifs);
	}
}

void Check_Image_File(PDV_STRUCT& pdv, std::ifstream& image_ifs, std::ifstream& zip_ifs) {

	// Vector "Image_Vec" stores the user's PNG image. Later, it will also store the contents of vectors "Script_Vec" and "Zip_Vec".
	pdv.Image_Vec.assign(std::istreambuf_iterator<char>(image_ifs), std::istreambuf_iterator<char>());

	pdv.image_size = pdv.Image_Vec.size();

	// Make sure we are dealing with a valid PNG image file.
	const std::string
		PNG_TOP_SIG = "\x89\x50\x4E\x47", 		  // PNG image header signature. 
		PNG_END_SIG = "\x49\x45\x4E\x44\xAE\x42\x60\x82", // PNG image end signature.
		GET_PNG_TOP_SIG{ pdv.Image_Vec.begin(), pdv.Image_Vec.begin() + PNG_TOP_SIG.length() },	// Attempt to get both image signatures from file stored in vector. 
		GET_PNG_END_SIG{ pdv.Image_Vec.end() - PNG_END_SIG.length(), pdv.Image_Vec.end() };

	// Make sure image has valid PNG signatures.
	if (GET_PNG_TOP_SIG != PNG_TOP_SIG || GET_PNG_END_SIG != PNG_END_SIG) {
		// Invalid image file, display error message and exit program.
		std::cerr << "\nImage File Error: File does not appear to be a valid PNG image.\n\n";
		std::exit(EXIT_FAILURE);
	}

	// Check a range of bytes within the "IHDR" chunk to make sure we have no "BAD_CHAR" characters that will break the Linux extraction script.
	// A script breaking character can appear within the width & height fields or the 4 byte CRC field of the "IHDR" chunk.
	// Manually modifying the dimensions (1% increase or decrease) of the image will usually resolve the issue. Repeat if necessary.

	int chunk_index = 18;

	// From index location, increment through 14 bytes of the IHDR chunk within vector "Image_Vec" and compare each byte to the 7 characters within "BAD_CHAR" string.
	while (chunk_index++ != 32) { // We start checking from the 19th character position of the IHDR chunk within vector "Image_Vec".
		for (int i = 0; i < 7; i++) {
			if (pdv.Image_Vec[chunk_index] == pdv.BAD_CHAR[i]) { // "BAD_CHAR" character found, display error message and exit program.
				std::cerr <<
					"\nImage File Error:\n\nThe IHDR chunk of this image contains a character that will break the Linux extraction script."
					"\nTry modifying image dimensions (1% increase or decrease) to resolve the issue. Repeat if necessary.\n\n";
				std::exit(EXIT_FAILURE);
			}
		}
	}

	// Now check for supported image dimensions and color types.
	const int
		IMAGE_WIDTH_DIMS = pdv.Image_Vec[18] << 8 | pdv.Image_Vec[19],		// Get width dimensions from vector.
		IMAGE_HEIGHT_DIMS = pdv.Image_Vec[22] << 8 | pdv.Image_Vec[23],		// Get height dimensions from vector.
		PNG_COLOR_TYPE = pdv.Image_Vec[25] == 6 ? 2 : pdv.Image_Vec[25];	// Get image color type value from vector. If value is 6 (Truecolor with alpha), set the value to 2 (Truecolor).

	constexpr int
		MAX_TRUECOLOR_DIMS = 899,	// 899 x 899 maximum supported dimensions for PNG Truecolor (PNG-32/24, color types 2 & 6).
		MAX_INDEXED_COLOR_DIMS = 4096,	// 4096 x 4096 maximum supported dimensions for PNG Indexed color (PNG-8, color type 3).
		MIN_DIMS = 68,			// 68 x 68 minimum supported dimensions for both PNG Indexed color and Truecolor.
		PNG_INDEXED_COLOR = 3,		// PNG-8, Indexed color value.
		PNG_TRUECOLOR = 2;		// PNG-24, Truecolour value. (We also use this value for PNG-32 (Truecolour with alpha 6), as we consider them the same for this program.

	const bool
		VALID_COLOR_TYPE = (PNG_COLOR_TYPE == PNG_INDEXED_COLOR) ? true		// Checking for valid color type of PNG image (PNG-32/24 Truecolor or PNG-8 Indexed color only).
		: ((PNG_COLOR_TYPE == PNG_TRUECOLOR) ? true : false),

		VALID_IMAGE_DIMS = (PNG_COLOR_TYPE == PNG_TRUECOLOR			// Checking for valid dimension size for PNG Truecolor (PNG-32/24) images.
			&& MAX_TRUECOLOR_DIMS >= IMAGE_WIDTH_DIMS
			&& MAX_TRUECOLOR_DIMS >= IMAGE_HEIGHT_DIMS
			&& IMAGE_WIDTH_DIMS >= MIN_DIMS
			&& IMAGE_HEIGHT_DIMS >= MIN_DIMS) ? true
		: ((PNG_COLOR_TYPE == PNG_INDEXED_COLOR					// Checking for valid dimension size for PNG Indexed color (PNG-8) images.
			&& MAX_INDEXED_COLOR_DIMS >= IMAGE_WIDTH_DIMS
			&& MAX_INDEXED_COLOR_DIMS >= IMAGE_HEIGHT_DIMS
			&& IMAGE_WIDTH_DIMS >= MIN_DIMS
			&& IMAGE_HEIGHT_DIMS >= MIN_DIMS) ? true : false);

	if (!VALID_COLOR_TYPE || !VALID_IMAGE_DIMS) {
		// Requirements check failure, display relevant error message and exit program.
		std::cerr << "\nImage File Error: " << (!VALID_COLOR_TYPE ? "Color type of PNG image is not supported.\n\nPNG-32/24 (Truecolor) or PNG-8 (Indexed Color) only"
			: "Dimensions of PNG image are not within the supported range.\n\nPNG-32/24 Truecolor: [68 x 68]<->[899 x 899].\nPNG-8 Indexed Color: [68 x 68]<->[4096 x 4096]") << ".\n\n";
		std::exit(EXIT_FAILURE);
	}

	// We appear to have a compatible PNG to use as our cover image. Now erase all unnecessary chunks.
	Erase_Image_Chunks(pdv, zip_ifs);
}

void Erase_Image_Chunks(PDV_STRUCT& pdv, std::ifstream& zip_ifs) {

	// Keep the critical PNG chunks: IHDR, *PLTE, IDAT & IEND.

	std::vector<Byte>Temp_Vec;

	// Copy the first 33 bytes of Image_Vec into Temp_Vec (PNG header + IHDR).
	Temp_Vec.insert(Temp_Vec.begin(), pdv.Image_Vec.begin(), pdv.Image_Vec.begin() + 33);

	const std::string IDAT_SIG = "IDAT";

	// Get first IDAT chunk index.
	size_t idat_index = std::search(pdv.Image_Vec.begin(), pdv.Image_Vec.end(), IDAT_SIG.begin(), IDAT_SIG.end()) - pdv.Image_Vec.begin() - 4;  // -4 is to position the index at the start of the IDAT chunk's length field.

	// Make sure this is a valid IDAT chunk. Check CRC value.

	// Get first IDAT chunk length value
	const size_t
		FIRST_IDAT_LENGTH = ((static_cast<size_t>(pdv.Image_Vec[idat_index]) << 24)
			| (static_cast<size_t>(pdv.Image_Vec[idat_index + 1]) << 16)
			| (static_cast<size_t>(pdv.Image_Vec[idat_index + 2]) << 8)
			| (static_cast<size_t>(pdv.Image_Vec[idat_index + 3]))),

		// Get first IDAT chunk's CRC index location.
		FIRST_IDAT_CRC_INDEX = idat_index + FIRST_IDAT_LENGTH + 8,

		FIRST_IDAT_CRC = ((static_cast<size_t>(pdv.Image_Vec[FIRST_IDAT_CRC_INDEX]) << 24)
			| (static_cast<size_t>(pdv.Image_Vec[FIRST_IDAT_CRC_INDEX + 1]) << 16)
			| (static_cast<size_t>(pdv.Image_Vec[FIRST_IDAT_CRC_INDEX + 2]) << 8)
			| (static_cast<size_t>(pdv.Image_Vec[FIRST_IDAT_CRC_INDEX + 3]))),

		CALC_FIRST_IDAT_CRC = Crc(&pdv.Image_Vec[idat_index + 4], FIRST_IDAT_LENGTH + 4);

	// Make sure values match.
	if (FIRST_IDAT_CRC != CALC_FIRST_IDAT_CRC) {
		std::cerr << "\nImage File Error: CRC value for first IDAT chunk is invalid.\n\n";
		std::exit(EXIT_FAILURE);
	}

	// *For PNG-8 Indexed color (3) we need to keep the PLTE chunk.
	if (Temp_Vec[25] == 3) {

		const std::string PLTE_SIG = "PLTE";

		// Find PLTE chunk index and copy its contents into Temp_Vec.
		const size_t PLTE_CHUNK_INDEX = std::search(pdv.Image_Vec.begin(), pdv.Image_Vec.end(), PLTE_SIG.begin(), PLTE_SIG.end()) - pdv.Image_Vec.begin() - 4;

		if (idat_index > PLTE_CHUNK_INDEX) {
			const size_t CHUNK_SIZE = ((static_cast<size_t>(pdv.Image_Vec[PLTE_CHUNK_INDEX + 1]) << 16)
				| (static_cast<size_t>(pdv.Image_Vec[PLTE_CHUNK_INDEX + 2]) << 8)
				| (static_cast<size_t>(pdv.Image_Vec[PLTE_CHUNK_INDEX + 3])));

			Temp_Vec.insert(Temp_Vec.end(), pdv.Image_Vec.begin() + PLTE_CHUNK_INDEX, pdv.Image_Vec.begin() + PLTE_CHUNK_INDEX + (CHUNK_SIZE + 12));
		}
		else {
			std::cerr << "\nImage File Error: Required PLTE chunk not found for Indexed-color (PNG-8) image.\n\n";
			std::exit(EXIT_FAILURE);
		}
	}

	// Find all the IDAT chunks and copy them into Temp_Vec.
	while (pdv.image_size != idat_index + 4) {
		const size_t CHUNK_SIZE = ((static_cast<size_t>(pdv.Image_Vec[idat_index]) << 24)
			| (static_cast<size_t>(pdv.Image_Vec[idat_index + 1]) << 16)
			| (static_cast<size_t>(pdv.Image_Vec[idat_index + 2]) << 8)
			| (static_cast<size_t>(pdv.Image_Vec[idat_index + 3])));

		Temp_Vec.insert(Temp_Vec.end(), pdv.Image_Vec.begin() + idat_index, pdv.Image_Vec.begin() + idat_index + (CHUNK_SIZE + 12));
		idat_index = std::search(pdv.Image_Vec.begin() + idat_index + 6, pdv.Image_Vec.end(), IDAT_SIG.begin(), IDAT_SIG.end()) - pdv.Image_Vec.begin() - 4;
	}

	// Copy the last 12 bytes of Image_Vec into Temp_Vec.
	Temp_Vec.insert(Temp_Vec.end(), pdv.Image_Vec.end() - 12, pdv.Image_Vec.end());

	Temp_Vec.swap(pdv.Image_Vec);

	// Update image size.
	pdv.image_size = pdv.Image_Vec.size();

	Check_Zip_File(pdv, zip_ifs);
}

void Check_Zip_File(PDV_STRUCT& pdv, std::ifstream& zip_ifs) {

	// Vector "Zip_Vec" will store the user's ZIP file. The contents of "Zip_Vec" will later be inserted into the vector "Image_Vec" as the last "IDAT" chunk. 
	// We will need to update the CRC value (last 4-bytes) and the chunk length field (first 4-bytes) within this vector. Both fields currently set to zero. 

	pdv.Zip_Vec = { 0x00, 0x00, 0x00, 0x00, 0x49, 0x44, 0x41, 0x54, 0x00, 0x00, 0x00, 0x00 };	// "IDAT" chunk name with 4-byte chunk length and crc fields.

	// Insert user's ZIP file into vector "Zip_Vec" from index 8, just after "IDAT" chunk name.
	pdv.Zip_Vec.insert(pdv.Zip_Vec.begin() + 8, std::istreambuf_iterator<char>(zip_ifs), std::istreambuf_iterator<char>());

	pdv.zip_size = pdv.Zip_Vec.size();

	// Location of "IDAT" chunk length field for vector "Zip_Vec".
	int
		chunk_length_index = 0,
		bits = 32;

	// Write the updated "IDAT" chunk length of vector "Zip_Vec" within its length field. 
	Value_Updater(pdv.Zip_Vec, chunk_length_index, pdv.zip_size - 12, bits, pdv.big_endian);

	const std::string
		ZIP_SIG = "\x50\x4B\x03\x04",	// Valid file signature of ZIP file.
		GET_ZIP_SIG{ pdv.Zip_Vec.begin() + 8, pdv.Zip_Vec.begin() + 8 + ZIP_SIG.length() };	// Get ZIP file signature from vector "Zip_Vec".

	constexpr int MIN_INZIP_NAME_LENGTH = 4;		// Set minimum filename length of zipped file. (1st filename record within ZIP archive).

	const int INZIP_NAME_LENGTH = pdv.Zip_Vec[34];		// Get length of zipped file name (1st file in ZIP record) from vector "Zip_Vec".

	if (GET_ZIP_SIG != ZIP_SIG || MIN_INZIP_NAME_LENGTH > INZIP_NAME_LENGTH) {
		// Display relevant error message and exit program.
		std::cerr << "\nZIP File Error: " << (GET_ZIP_SIG != ZIP_SIG ? "File does not appear to be a valid ZIP archive"
			: "\n\nName length of first file within ZIP archive is too short.\nIncrease its length (minimum 4 characters) and make sure it has a valid extension") << ".\n\n";
		std::exit(EXIT_FAILURE);
	}
	Complete_Extraction_Script(pdv);
}

void Complete_Extraction_Script(PDV_STRUCT& pdv) {

	/* Vector "Script_Vec" (See "script_info.txt" in this repo).

	First 4 bytes of the vector is the chunk length field, followed by chunk name "iCCP" then our barebones extraction script.

	This vector stores the shell/batch script used for extracting and opening the embedded zipped file (First filename within the ZIP file record).
	The barebones script is about 300 bytes. The script size limit is 750 bytes, which should be more than enough to account
	for the later addition of filenames, application & argument strings, plus other required script commands.

	Script supports both Linux & Windows. The completed script, when executed, will unzip the archive within the
	PNG image and (depending on file type) attempt to open/display/play/run the first filename within the ZIP file record by using an application
	command based on the matched file extension, or if no match found, defaulting to the operating system making the choice.

	The zipped file needs to be compatible with the operating system you are running it on.
	The completed script within the "iCCP" chunk will later be inserted into the vector "Image_Vec" which contains the user's PNG image file */

	pdv.Script_Vec = {
			0x00, 0x00, 0x00, 0xFD, 0x69, 0x43, 0x43, 0x50, 0x73, 0x63, 0x72, 0x00, 0x00, 0x0D, 0x52,
			0x45, 0x4D, 0x3B, 0x63, 0x6C, 0x65, 0x61, 0x72, 0x3B, 0x6D, 0x6B, 0x64, 0x69, 0x72, 0x20,
			0x2E, 0x2F, 0x70, 0x64, 0x76, 0x7A, 0x69, 0x70, 0x5F, 0x65, 0x78, 0x74, 0x72, 0x61, 0x63,
			0x74, 0x65, 0x64, 0x3B, 0x6D, 0x76, 0x20, 0x22, 0x24, 0x30, 0x22, 0x20, 0x2E, 0x2F, 0x70,
			0x64, 0x76, 0x7A, 0x69, 0x70, 0x5F, 0x65, 0x78, 0x74, 0x72, 0x61, 0x63, 0x74, 0x65, 0x64,
			0x3B, 0x63, 0x64, 0x20, 0x2E, 0x2F, 0x70, 0x64, 0x76, 0x7A, 0x69, 0x70, 0x5F, 0x65, 0x78,
			0x74, 0x72, 0x61, 0x63, 0x74, 0x65, 0x64, 0x3B, 0x75, 0x6E, 0x7A, 0x69, 0x70, 0x20, 0x2D,
			0x71, 0x6F, 0x20, 0x22, 0x24, 0x30, 0x22, 0x3B, 0x63, 0x6C, 0x65, 0x61, 0x72, 0x3B, 0x22,
			0x22, 0x3B, 0x65, 0x78, 0x69, 0x74, 0x3B, 0x0D, 0x0A, 0x23, 0x26, 0x63, 0x6C, 0x73, 0x26,
			0x6D, 0x6B, 0x64, 0x69, 0x72, 0x20, 0x2E, 0x5C, 0x70, 0x64, 0x76, 0x7A, 0x69, 0x70, 0x5F,
			0x65, 0x78, 0x74, 0x72, 0x61, 0x63, 0x74, 0x65, 0x64, 0x26, 0x6D, 0x6F, 0x76, 0x65, 0x20,
			0x22, 0x25, 0x7E, 0x64, 0x70, 0x6E, 0x78, 0x30, 0x22, 0x20, 0x2E, 0x5C, 0x70, 0x64, 0x76,
			0x7A, 0x69, 0x70, 0x5F, 0x65, 0x78, 0x74, 0x72, 0x61, 0x63, 0x74, 0x65, 0x64, 0x26, 0x63,
			0x64, 0x20, 0x2E, 0x5C, 0x70, 0x64, 0x76, 0x7A, 0x69, 0x70, 0x5F, 0x65, 0x78, 0x74, 0x72,
			0x61, 0x63, 0x74, 0x65, 0x64, 0x26, 0x63, 0x6C, 0x73, 0x26, 0x74, 0x61, 0x72, 0x20, 0x2D,
			0x78, 0x66, 0x20, 0x22, 0x25, 0x7E, 0x6E, 0x30, 0x25, 0x7E, 0x78, 0x30, 0x22, 0x26, 0x20,
			0x22, 0x22, 0x26, 0x72, 0x65, 0x6E, 0x20, 0x22, 0x25, 0x7E, 0x6E, 0x30, 0x25, 0x7E, 0x78,
			0x30, 0x22, 0x20, 0x2A, 0x2E, 0x70, 0x6E, 0x67, 0x26, 0x65, 0x78, 0x69, 0x74, 0x0D, 0x0A,
			0x00, 0x00, 0x00, 0x00 };

	// "App_Vec" string vector. 
	// Stores file extensions for some popular media types, along with several default application commands (+ args) that support those extensions.
	// These vector string elements will be used in the completion of our extraction script.

	std::vector<std::string> App_Vec{ "aac", "mp3", "mp4", "avi", "asf", "flv", "ebm", "mkv", "peg", "wav", "wmv", "wma", "mov", "3gp", "ogg", "pdf", ".py", "ps1", "exe",
		".sh", "vlc --play-and-exit --no-video-title-show ", "evince ", "python3 ", "pwsh ", "./", "xdg-open ", "powershell;Invoke-Item ",
		" &> /dev/null", "start /b \"\"", "pause&", "powershell", "chmod +x ", ";" };

	constexpr int
		FIRST_ZIP_NAME_REC_LENGTH_INDEX = 34,	// "Zip_Vec" vector's index location for the length value of the zipped filename (First filename within the ZIP file record).
		FIRST_ZIP_NAME_REC_INDEX = 38,		// "Zip_Vec" start index location for the zipped filename.

		// "App_Vec" vector element index values. 
		// Some "App_Vec" vector string elements are added later (via emplace_back) so they don't currently appear in the above string vector.

		VIDEO_AUDIO = 20,		// "vlc" app command for Linux. 
		PDF = 21,			// "evince" app command for Linux. 
		PYTHON = 22,			// "python3" app command for Linux & Windows.
		POWERSHELL = 23,		// "pwsh" app command for Linux, for starting PowerShell scripts.
		EXECUTABLE = 24,		// "./" prepended to filename. Required when running Linux executables.
		BASH_XDG_OPEN = 25,		// "xdg-open" Linux command, runs shell scripts (.sh), opens folders & unmatched file extensions.
		FOLDER_INVOKE_ITEM = 26,	// "powershell;Invoke-Item" command used in Windows for opening zipped folders, instead of files.
		START_B = 28,			// "start /b" Windows command used to open most file types. Windows uses set default app for most file types.
		WIN_POWERSHELL = 30,		// "powershell" commmand used by Windows for running PowerShell scripts.
		PREPEND_FIRST_ZIP_NAME_REC = 36;	// first_zip_name with ".\" prepended characters. Required for Windows PowerShell, e.g. powershell ".\my_ps_script.ps1".

	const int FIRST_ZIP_NAME_REC_LENGTH = pdv.Zip_Vec[FIRST_ZIP_NAME_REC_LENGTH_INDEX];	// Get character length of the zipped media filename from vector "Zip_Vec".

	std::string
		// Get the zipped filename string from vector "Zip_Vec". (First filename within the ZIP record).
		first_zip_name{ pdv.Zip_Vec.begin() + FIRST_ZIP_NAME_REC_INDEX, pdv.Zip_Vec.begin() + FIRST_ZIP_NAME_REC_INDEX + FIRST_ZIP_NAME_REC_LENGTH },

		// Get the file extension from the zipped filename.
		first_zip_name_ext = first_zip_name.substr(first_zip_name.length() - 3, 3),

		// Variables for optional command-line arguments.
		args_linux{},
		args_windows{};

	// Check for "." character to see if the "first_zip_name" has a file extension.
	const auto CHECK_FILE_EXT = first_zip_name.find_last_of('.');

	// Store this filename (first filename within the ZIP record) in "App_Vec" vector (33).
	App_Vec.emplace_back(first_zip_name);

	// When inserting string elements from vector "App_Vec" into the script (within vector "Script_Vec"), we are adding items in 
	// the order of last to first. The Windows script is completed first, followed by Linux. 
	// This order prevents the vector insert locations from changing every time we add a new string element into the vector.

	// The vector "Sequence_Vec" can be split into four sequences containing "Script_Vec" index values (high numbers) used by the 
	// "insert_index" variable and the corresponding "App_Vec" index values (low numbers) used by the "app_index" variable.

	// For example, in the 1st sequence, Sequence_Vec[0] = index 241 of "Script_Vec" ("insert_index") corresponds with
	// Sequence_Vec[5] = "App_Vec" index 33 ("app_index"), which is the "first_zip_name" string element (first filename within the ZIP record). 
	// "App_Vec" string element 33 (first_zip_name) will be inserted into the script (vector "Script_Vec") at index 241.

	int
		app_index = 0,		// Uses the "App_Vec" index values from the vector Sequence_Vec.
		insert_index = 0,	// Uses the "Script_Vec" index values from vector Sequence_Vec.
		sequence_limit = 0;	// Stores the length limit of each of the four sequences. 

	std::vector<int>Sequence_Vec{
			241, 239, 121, 120, 119,	// 1st sequence for case "VIDEO_AUDIO".
			33, 28, 27, 33, 20,

			241, 239, 120, 119,		// 2nd sequence for cases "PDF, FOLDER_INVOKE_ITEM, DEFAULT".
			33, 28, 33, 21,

			264, 242, 241, 239, 121, 120, 119,			// 3rd sequence for cases "PYTHON, POWERSHELL".
			29, 35, 33, 22, 34, 33, 22,

			264, 242, 241, 239, 121, 120, 119, 119, 119, 119,	// 4th sequence for cases "EXECUTABLE & BASH_XDG_OPEN".
			29, 35, 33, 28, 34, 33, 24, 32, 33, 31 };

	/*  	[Sequence_Vec](insert_index)[Sequence_Vec](app_index)
		Build script example below is using the first sequence (see vector "Sequence_Vec" above).

		VIDEO_AUDIO:
		[0]>(241)[5]>(33) Windows: "Image_Vec" 241 insert index for the string variable first_zip_name, "App_Vec" 33.
		[1]>(239)[6]>(28) Windows: "Image_Vec" 239 insert index for the string "start /b", "App_Vec" 28.
		[2]>(121)[7]>(27) Linux: "Image_Vec" 121 insert index for the string "Dev Null", "App_Vec" 27.
		[3]>(120)[8]>(33) Linux: "Image_Vec" 120 insert index for the the string variable first_zip_name, "App_Vec" 33.
		[4]>(119)[9]>(20) Linux: "Image_Vec" 119 insert index for the string "vlc", "App_Vec" 20.
		Sequence limit is 5 (value taken from first app_index value for each sequence).

		Matching a file extension from the string variable "first_zip_name_ext" within "App_Vec" (33), we can select which application string and commands to use
		in our extraction script, which when executed, will (depending on file type) open/display/play/run the extracted zipped file ("first_zip_name").

		Once the correct app extension has been matched by the "for-loop" below, it passes the app_index result to the switch statement.
		The relevant Case sequence is then used in completing the extraction script within vector "Script_Vec".	*/

	for (; app_index != 26; app_index++) {
		if (App_Vec[app_index] == first_zip_name_ext) {
			// After a file extension match, any app_index value between 0 and 14 defaults to "App_Vec" 20 (vlc / VIDEO_AUDIO).
			// If over 14, we add 6 to the value. 15 + 6 = "App_Vec" 21 (evince for PDF (Linux) ), 16 + 6 = "App_Vec" 22 (python3/.py), etc.
			app_index = app_index <= 14 ? 20 : app_index += 6;
			break;
		}
	}

	// If no file extension detected, check if "first_zip_name" points to a folder (/), else assume file is a Linux executable.
	if (CHECK_FILE_EXT == 0 || CHECK_FILE_EXT > first_zip_name.length()) {
		app_index = pdv.Zip_Vec[FIRST_ZIP_NAME_REC_INDEX + FIRST_ZIP_NAME_REC_LENGTH - 1] == '/' ? FOLDER_INVOKE_ITEM : EXECUTABLE;
	}

	// Provide the user with the option to add command-line arguments for file types: 
	// Python (.py), PowerShell (.ps1), Shell script (.sh) and executable (.exe). (no extension, defaults to .exe, if not a folder).
	// The provided arguments for your file type will be stored within the PNG image, along with the extraction script.

	if (app_index > 21 && app_index < 26) {
		std::cout << "\nFor this file type you can provide command-line arguments here, if required.\n\nLinux: ";
		std::getline(std::cin, args_linux);
		std::cout << "\nWindows: ";
		std::getline(std::cin, args_windows);

		args_linux.insert(0, "\x20"),
		args_windows.insert(0, "\x20");

		App_Vec.emplace_back(args_linux),	// "App_Vec" (34).
		App_Vec.emplace_back(args_windows);	// "App_Vec" (35).
	}

	std::cout << "\nUpdating extraction script.\n";

	switch (app_index) {
		case VIDEO_AUDIO:	// Case uses 1st sequence: [241,239,121,120,119] , [33,28,27,33,20].
			app_index = 5;
			break;
		case PDF:		// These two cases (with minor changes) use the 2nd sequence: [241,239,120,119] , [33,28,33,21].
		case FOLDER_INVOKE_ITEM:
			Sequence_Vec[15] = app_index == FOLDER_INVOKE_ITEM ? FOLDER_INVOKE_ITEM : START_B;
			Sequence_Vec[17] = app_index == FOLDER_INVOKE_ITEM ? BASH_XDG_OPEN : PDF;
			insert_index = 10;
			app_index = 14;
			break;
		case PYTHON:		// These two cases (with some changes) use the 3rd sequence: [264,242,241,239,121,120,119] , [29,35,33,22,34,33,22].
		case POWERSHELL:
			if (app_index == POWERSHELL) {
				first_zip_name.insert(0, ".\\");		//  ".\" prepend to "first_zip_name". Required for Windows PowerShell, e.g. powershell ".\my_ps_script.ps1".
				App_Vec.emplace_back(first_zip_name);		// Add the filename with the prepended ".\" to the "AppVec" vector (36).
				Sequence_Vec[31] = POWERSHELL;			// Swap index number to Linux PowerShell (pwsh 23)
				Sequence_Vec[28] = WIN_POWERSHELL;		// Swap index number to Windows PowerShell (powershell 30)
				Sequence_Vec[27] = PREPEND_FIRST_ZIP_NAME_REC;	// Swap index number to PREPEND_FIRST_ZIP_NAME_REC (36), used with the Windows powershell command.
			}
			insert_index = 18;
			app_index = 25;
			break;
		case EXECUTABLE:	// These two cases (with minor changes) use the 4th sequence: [264,242,241,239,121,120,119,119,119,119] , [29,35,33,28,34,33,24,32,33,31].
		case BASH_XDG_OPEN:
			insert_index = app_index == EXECUTABLE ? 32 : 33;
			app_index = insert_index == 32 ? 42 : 43;
			break;
		default:			// Unmatched file extensions. Rely on operating system to use the set default program for dealing with unknown file types.
			insert_index = 10;	// Default uses 2nd sequence, we just need to alter one index number.
			app_index = 14;
			Sequence_Vec[17] = BASH_XDG_OPEN;	// Swap index number to BASH_XDG_OPEN (25)
	}

	// Set the sequence_limit variable using the first app_index value from each switch case sequence.
	// Reduce sequence_limit variable value by 1 if insert_index is 33 (case BASH_XDG_OPEN).

	sequence_limit = insert_index == 33 ? app_index - 1 : app_index;

	// With just a single vector insert command within the "while-loop", we can insert all the required strings into the extraction script (vector "Script_Vec"), 
	// based on the sequence, which is selected by the relevant Case from the above switch statement after the extension match. 

	while (sequence_limit > insert_index) {
		pdv.Script_Vec.insert(pdv.Script_Vec.begin() + Sequence_Vec[insert_index], App_Vec[Sequence_Vec[app_index]].begin(), App_Vec[Sequence_Vec[app_index]].end());
		insert_index++;
		app_index++;
	}

	pdv.script_size = pdv.Script_Vec.size();

	int
		chunk_length_index = 2,	// "Script_Vec" vector's index location for the "iCCP" chunk length field.
		bits = 16;

	// Call function to write updated chunk length value for the "iCCP" chunk into its length field. 
	// Due to its small size, the "iCCP" chunk will only use 2 bytes maximum (bits=16) of the 4 byte length field.

	Value_Updater(pdv.Script_Vec, chunk_length_index, pdv.script_size - 12, bits, pdv.big_endian);

	// Check the first byte of the "iCCP" chunk length field to make sure the updated chunk length does not match 
	// any of the "BAD_CHAR" characters that will break the Linux extraction script.

	for (int i = 0; i < 7; i++) {
		if (pdv.Script_Vec[3] == pdv.BAD_CHAR[i]) {

			// "BAD_CHAR" found. Insert 10 bytes "." at the end of "Script_Vec" to increase chunk length. Update chunk length field. 
			// This should now skip over any BAD_CHAR characters, regardless of the chunk size (within its size limit).

			const std::string INCREASE_LENGTH_STRING = "..........";

			pdv.Script_Vec.insert(pdv.Script_Vec.begin() + pdv.script_size - 4, INCREASE_LENGTH_STRING.begin(), INCREASE_LENGTH_STRING.end());

			pdv.script_size = pdv.Script_Vec.size();

			Value_Updater(pdv.Script_Vec, chunk_length_index, pdv.script_size - 12, bits, pdv.big_endian); // Update size again.

			break;
		}
	}

	pdv.combined_file_size = pdv.Script_Vec.size() + pdv.Image_Vec.size() + pdv.Zip_Vec.size();

	constexpr int
		MAX_SCRIPT_SIZE = 750,
		ICCP_CHUNK_INDEX = 4;

	// Display relevant error message and exit program if extraction script exceeds size limit.
	if (pdv.script_size > MAX_SCRIPT_SIZE || pdv.combined_file_size > pdv.MAX_FILE_SIZE) {
		std::cerr << "\nFile Size Error: " << (pdv.script_size > MAX_SCRIPT_SIZE ? "Extraction script exceeds size limit"
			: "The combined file size of your PNG image, ZIP file and Extraction Script, exceeds file size limit") << ".\n\n";
		std::exit(EXIT_FAILURE);
	}

	// Now the "iCCP" chunk is complete with the extraction script, we need to update the chunk's CRC value.
	// Pass these two values (ICCP_CHUNK_INDEX & iCCP chunk size (script_size) - 8) to the CRC function to get correct "iCCP" chunk CRC value.

	const size_t ICCP_CHUNK_CRC = Crc(&pdv.Script_Vec[ICCP_CHUNK_INDEX], pdv.script_size - 8);

	// Get vector index location for the "iCCP" chunk's CRC field.
	size_t iccp_crc_index = pdv.script_size - 4;

	bits = 32;

	// Write the updated CRC value into the "iCCP" chunk's CRC field (bits=32) within vector "Script_Vec".
	Value_Updater(pdv.Script_Vec, iccp_crc_index, ICCP_CHUNK_CRC, bits, pdv.big_endian);

	// Insert vectors "Scrip_Vec" ("iCCP" chunk with completed extraction script) & "Zip_Vec" ("IDAT" chunk with ZIP file) into vector "Image_Vec" (PNG image).
	Combine_Vectors(pdv);
}

void Combine_Vectors(PDV_STRUCT& pdv) {

	// This value will be used as the insert location within vector "Image_Vec" for contents of vector "Script_Vec". 
	// Script_Vec's inserted contents will appear within the "iCCP" chunk, just after the "IHDR" chunk of "Image_Vec".

	constexpr int FIRST_IDAT_INDEX = 33;

	std::cout << "\nEmbedding extraction script within the PNG image.\n";

	// Insert contents of vector "Script_Vec" ("iCCP" chunk containing the extraction script) into vector "Image_Vec".	
	pdv.Image_Vec.insert((pdv.Image_Vec.begin() + FIRST_IDAT_INDEX), pdv.Script_Vec.begin(), pdv.Script_Vec.end());

	std::cout << "\nEmbedding ZIP file within the PNG image.\n";

	// Insert contents of vector "Zip_Vec" ("IDAT" chunk with ZIP file) into vector "Image_Vec".
	// This now becomes the new last "IDAT" chunk of the PNG image within vector "Image_Vec".

	pdv.Image_Vec.insert((pdv.Image_Vec.end() - 12), pdv.Zip_Vec.begin(), pdv.Zip_Vec.end());

	const size_t IDAT_ZIP_INDEX = pdv.image_size + pdv.script_size - 8;

	// Before updating the last "IDAT" chunk's CRC value, adjust ZIP file offsets within this chunk, to their new locations, so that the ZIP file continues to be valid & extractable.
	Fix_Zip_Offset(pdv, IDAT_ZIP_INDEX);

	// Get CRC value for our (new) last "IDAT" chunk.
	const size_t IDAT_ZIP_CRC = Crc(&pdv.Image_Vec[IDAT_ZIP_INDEX], pdv.zip_size - 8); // We don't include the length or CRC fields (-8 bytes).

	pdv.image_size = pdv.Image_Vec.size();

	size_t crc_insert_index = pdv.image_size - 16;	// Get index location for last "IDAT" chunk's 4-byte CRC field, from vector "Image_Vec".

	int bits = 32;

	pdv.big_endian = true;

	// Write new CRC value into the last "IDAT" chunk's CRC index field, within the vector "Image_Vec".
	Value_Updater(pdv.Image_Vec, crc_insert_index, IDAT_ZIP_CRC, bits, pdv.big_endian);

	Write_Out_Polyglot_File(pdv);
}

void Fix_Zip_Offset(PDV_STRUCT& pdv, const size_t& IDAT_ZIP_INDEX) {

	const std::string
		START_CENTRAL_DIR_SIG = "PK\x01\x02",
		END_CENTRAL_DIR_SIG = "PK\x05\x06",
		ZIP_SIG = "PK\x03\x04";
	
	// Search vector "Image_Vec" (start from last "IDAT" chunk index) to get locations for "Start Central Directory" & "End Central Directory".
	const size_t
		START_CENTRAL_DIR_INDEX = std::search(pdv.Image_Vec.begin() + IDAT_ZIP_INDEX, pdv.Image_Vec.end(), START_CENTRAL_DIR_SIG.begin(), START_CENTRAL_DIR_SIG.end()) - pdv.Image_Vec.begin(),
		END_CENTRAL_DIR_INDEX = std::search(pdv.Image_Vec.begin() + START_CENTRAL_DIR_INDEX, pdv.Image_Vec.end(), END_CENTRAL_DIR_SIG.begin(), END_CENTRAL_DIR_SIG.end()) - pdv.Image_Vec.begin();

	size_t
		zip_records_index = END_CENTRAL_DIR_INDEX + 11,		// Index location for ZIP file records value.
		comment_length_index = END_CENTRAL_DIR_INDEX + 21,	// Index location for ZIP comment length.
		end_central_start_index = END_CENTRAL_DIR_INDEX + 19,	// Index location for End Central Start offset.
		central_local_index = START_CENTRAL_DIR_INDEX - 1,	// Initialise variable to just before Start Central index location.
		new_offset = IDAT_ZIP_INDEX;				// Initialise variable to start index of last IDAT chunk, which contains user's ZIP file.

	int 
		bits = 32,
		zip_records = (pdv.Image_Vec[zip_records_index] << 8) | pdv.Image_Vec[zip_records_index - 1];	// Get ZIP file records value from index location of vector "Image_Vec".
	
	pdv.big_endian = false;

	// Starting from the last "IDAT" chunk, search for ZIP file record offsets and update them to their new offset location.
	while (zip_records--) {
		new_offset = std::search(pdv.Image_Vec.begin() + new_offset + 1, pdv.Image_Vec.end(), ZIP_SIG.begin(), ZIP_SIG.end()) - pdv.Image_Vec.begin(),
		central_local_index = 45 + std::search(pdv.Image_Vec.begin() + central_local_index, pdv.Image_Vec.end(), START_CENTRAL_DIR_SIG.begin(), START_CENTRAL_DIR_SIG.end()) - pdv.Image_Vec.begin();
		Value_Updater(pdv.Image_Vec, central_local_index, new_offset, bits, pdv.big_endian);
	}
	
	// Write updated "Start Central Directory" offset into End Central Directory's "Start Central Directory" index location within vector "Image_Vec".
	Value_Updater(pdv.Image_Vec, end_central_start_index, START_CENTRAL_DIR_INDEX, bits, pdv.big_endian);

	// JAR file support. Get global comment length value from ZIP file within vector "Image_Vec" and increase it by 16 bytes to cover end of PNG file.
	// To run a JAR file, you will need to rename the '.png' extension to '.jar'.  
	// or run the command: "java -jar image_file_name.png"

	int comment_length = 16 + (static_cast<size_t>(pdv.Image_Vec[comment_length_index] << 8) | (static_cast<size_t>(pdv.Image_Vec[comment_length_index - 1])));

	bits = 16;

	// Write the ZIP comment length value into the comment length index location within vector "Image_Vec".
	Value_Updater(pdv.Image_Vec, comment_length_index, comment_length, bits, pdv.big_endian);
}

void Write_Out_Polyglot_File(PDV_STRUCT& pdv) {

	srand((unsigned)time(NULL));  // For output filename.

	const std::string
		NAME_VALUE = std::to_string(rand()),
		PDV_FILENAME = "pzip_" + NAME_VALUE.substr(0, 5) + ".png"; // Unique filename for the complete polyglot image.

	std::ofstream file_ofs(PDV_FILENAME, std::ios::binary);

	if (!file_ofs) {
		std::cerr << "\nWrite File Error: Unable to write to file.\n\n";
		std::exit(EXIT_FAILURE);
	}

	std::cout << "\nWriting ZIP embedded PNG image out to disk.\n";

	// Write out to file vector "Image_Vec" now containing the completed polyglot image (Image + Script + ZIP).
	file_ofs.write((char*)&pdv.Image_Vec[0], pdv.image_size);

	std::cout << "\nSaved PNG image: " + PDV_FILENAME + '\x20' + std::to_string(pdv.image_size) + " Bytes.\n\nComplete!\n\nYou can now share your PNG-ZIP polyglot image on the relevant supported platforms.\n\n";
}

// The following code (slightly modified) to compute CRC32 (for "IDAT" & "iCCP" chunks) was taken from: https://www.w3.org/TR/2003/REC-PNG-20031110/#D-CRCAppendix 
size_t Crc_Update(const size_t& Crc, Byte* buf, const size_t& len) {
	// Table of CRCs of all 8-bit messages.
	constexpr size_t Crc_Table[256]{
		0x00, 	    0x77073096, 0xEE0E612C, 0x990951BA, 0x76DC419,  0x706AF48F, 0xE963A535, 0x9E6495A3, 0xEDB8832,  0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x9B64C2B,  0x7EB17CBD,
		0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
		0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
		0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
		0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x1DB7106,  0x98D220BC, 0xEFD5102A, 0x71B18589, 0x6B6B51F,
		0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0xF00F934,  0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x86D3D2D,  0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
		0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE,
		0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
		0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
		0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x3B6E20C,  0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x4DB2615,  0x73DC1683, 0xE3630B12, 0x94643B84, 0xD6D6A3E,  0x7A6A5AA8,
		0xE40ECF0B, 0x9309FF9D, 0xA00AE27,  0x7D079EB1, 0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0,
		0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
		0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703,
		0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x26D930A,
		0x9C0906A9, 0xEB0E363F, 0x72076785, 0x5005713,  0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0xCB61B38,  0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0xBDBDF21,  0x86D3D2D4, 0xF1D4E242,
		0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777, 0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
		0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5,
		0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
		0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D };

	// Update a running CRC with the bytes buf[0..len - 1] the CRC should be initialized to all 1's, 
	// and the transmitted value is the 1's complement of the final running CRC (see the crc() routine below).
	size_t c = Crc;

	for (int n = 0; n < len; n++) {
		c = Crc_Table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
	}
	return c;
}

// Return the CRC of the bytes buf[0..len-1].
size_t Crc(Byte* buf, const size_t& len)
{
	return Crc_Update(0xffffffffL, buf, len) ^ 0xffffffffL;
}

void Value_Updater(std::vector<Byte>& vec, size_t value_insert_index, const size_t& NEW_VALUE, int bits, bool big_endian) {

	if (big_endian) {
		while (bits) {
			static_cast<size_t>(vec[value_insert_index++] = (NEW_VALUE >> (bits -= 8)) & 0xff);
		}
	}
	else {
		while (bits) {
			static_cast<size_t>(vec[value_insert_index--] = (NEW_VALUE >> (bits -= 8)) & 0xff);
		}
	}
}

void Display_Info() {

	std::cout << R"(
PNG Data Vehicle ZIP Edition (PDVZIP v1.8). Created by Nicholas Cleasby (@CleasbyCode) 6/08/2022.
		
PDVZIP enables you to embed a ZIP file within a *tweetable and "executable" PNG image.  		
		
The hosting sites will retain the embedded arbitrary data within the PNG image.  
		
PNG image size limits are platform dependant:  

Flickr (200MB), Imgbb (32MB), PostImage (24MB), ImgPile (8MB), Twitter (5MB).

Once the ZIP file has been embedded within a PNG image, it can be shared on your chosen
hosting site or 'executed' whenever you want to access the embedded file(s).

From a Linux terminal: ./pdvzip_your_image.png (Image file requires executable permissions).
From a Windows terminal: First, rename the '.png' file extension to '.cmd', then .\pdvzip_your_image.cmd 

For common video/audio files, Linux requires 'vlc (VideoLAN)', Windows uses the set default media player.
PDF '.pdf', Linux requires the 'evince' program, Windows uses the set default PDF viewer.
Python '.py', Linux & Windows use the 'python3' command to run these programs.
PowerShell '.ps1', Linux uses the 'pwsh' command (if PowerShell installed), Windows uses 'powershell' to run these scripts.

For any other media type/file extension, Linux & Windows will rely on the operating system's set default application.

PNG Image Requirements for Arbitrary Data Preservation

PNG file size (image + embedded content) must not exceed the hosting site's size limits.
The site will either refuse to upload your image or it will convert your image to jpg, such as Twitter.

Dimensions:

The following dimension size limits are specific to pdvzip and not necessarily the extact hosting site's size limits.

PNG-32 (Truecolour with alpha [6])
PNG-24 (Truecolour [2])

Image dimensions can be set between a minimum of 68 x 68 and a maximum of 899 x 899.
These dimension size limits are for compatibility reasons, allowing it to work with all the above listed platforms.

Note: Images that are created & saved within your image editor as PNG-32/24 that are either 
black & white/grayscale, images with 256 colours or less, will be converted by Twitter to 
PNG-8 and you will lose the embedded content. If you want to use a simple "single" colour PNG-32/24 image,
then fill an area with a gradient colour instead of a single solid colour.
Twitter should then keep the image as PNG-32/24.

PNG-8 (Indexed-colour [3])

Image dimensions can be set between a minimum of 68 x 68 and a maximum of 4096 x 4096.

Chunks:

For example, with Twitter, you can overfill the following PNG chunks with arbitrary data, 
in which the platform will preserve as long as you keep within the image dimension & file size limits.  

Other platforms may differ in what chunks they preserve and which you can overfill.

bKGD, cHRM, gAMA, hIST,
iCCP, (Only 10KB max. with Twitter).
IDAT, (Use as last IDAT chunk, after the final image IDAT chunk).
PLTE, (Use only with PNG-32 & PNG-24 for arbitrary data).
pHYs, sBIT, sPLT, sRGB,
tRNS. (Not recommended, may distort image).

This program uses the iCCP (extraction script) and IDAT (zip file) chunk names for storing arbitrary data.

ZIP File Size & Other Information

To work out the maximum ZIP file size, start with the hosting site's size limit,  
minus your PNG image size, minus 750 bytes (internal shell extraction script size).   

Twitter example: (5MB) 5,242,880 - (307,200 + 750) = 4,934,930 bytes available for your ZIP file.

The less detailed the image, the more space available for the ZIP.

Make sure ZIP file is a standard ZIP archive, compatible with Linux unzip & Windows Explorer.
Do not include other .zip files within the main ZIP archive. (.rar files are ok).
Do not include other pdvzip created PNG image files within the main ZIP archive, as they are essentially .zip files.
Use file extensions for your media file within the ZIP archive: my_doc.pdf, my_video.mp4, my_program.py, etc.
A file without an extension will be treated as a Linux executable.
Paint.net application is recommended for easily creating compatible PNG image files.
 
)";
}
