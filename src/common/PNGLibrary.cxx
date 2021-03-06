//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2017 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#include <fstream>
#include <cmath>

#include "bspf.hxx"
#include "FrameBuffer.hxx"
#include "Props.hxx"
#include "PNGLibrary.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PNGLibrary::PNGLibrary(const FrameBuffer& fb)
  : myFB(fb)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::loadImage(const string& filename, FBSurface& surface)
{
  #define loadImageERROR(s) { err_message = s; goto done; }

  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  png_uint_32 iwidth, iheight;
  int bit_depth, color_type, interlace_type;
  const char* err_message = nullptr;

  ifstream in(filename, std::ios_base::binary);
  if(!in.is_open())
    loadImageERROR("No image found");

  // Create the PNG loading context structure
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                 png_user_error, png_user_warn);
  if(png_ptr == nullptr)
    loadImageERROR("Couldn't allocate memory for PNG file");

  // Allocate/initialize the memory for image information.  REQUIRED.
	info_ptr = png_create_info_struct(png_ptr);
  if(info_ptr == nullptr)
    loadImageERROR("Couldn't create image information for PNG file");

  // Set up the input control
  png_set_read_fn(png_ptr, &in, png_read_data);

  // Read PNG header info
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &iwidth, &iheight, &bit_depth,
    &color_type, &interlace_type, nullptr, nullptr);

  // Tell libpng to strip 16 bit/color files down to 8 bits/color
  png_set_strip_16(png_ptr);

  // Extract multiple pixels with bit depths of 1, 2, and 4 from a single
  // byte into separate bytes (useful for paletted and grayscale images).
  png_set_packing(png_ptr);

  // Only normal RBG(A) images are supported (without the alpha channel)
  if(color_type == PNG_COLOR_TYPE_RGBA)
  {
    png_set_strip_alpha(png_ptr);
  }
  else if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
  {
    loadImageERROR("Greyscale PNG images not supported");
  }
  else if(color_type == PNG_COLOR_TYPE_PALETTE)
  {
    png_set_palette_to_rgb(png_ptr);
  }
  else if(color_type != PNG_COLOR_TYPE_RGB)
  {
    loadImageERROR("Unknown format in PNG image");
  }

  // Create/initialize storage area for the current image
  if(!allocateStorage(iwidth, iheight))
    loadImageERROR("Not enough memory to read PNG file");

  // The PNG read function expects an array of rows, not a single 1-D array
  for(uInt32 irow = 0, offset = 0; irow < ReadInfo.height; ++irow, offset += ReadInfo.pitch)
    ReadInfo.row_pointers[irow] = png_bytep(ReadInfo.buffer.get() + offset);

  // Read the entire image in one go
  png_read_image(png_ptr, ReadInfo.row_pointers.get());

  // We're finished reading
  png_read_end(png_ptr, info_ptr);

  // Load image into the surface, setting the correct dimensions
  loadImagetoSurface(surface);

  // Cleanup
done:
  if(png_ptr)
    png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : 0, 0);

  if(err_message)
    throw runtime_error(err_message);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::saveImage(const string& filename, const VariantList& comments)
{
  ofstream out(filename, std::ios_base::binary);
  if(!out.is_open())
    throw runtime_error("ERROR: Couldn't create snapshot file");

  const GUI::Rect& rect = myFB.imageRect();
  png_uint_32 width = rect.width(), height = rect.height();

  // Get framebuffer pixel data (we get ABGR format)
  unique_ptr<png_byte[]> buffer = make_unique<png_byte[]>(width * height * 4);
  myFB.readPixels(buffer.get(), width*4, rect);

  // Set up pointers into "buffer" byte array
  unique_ptr<png_bytep[]> rows = make_unique<png_bytep[]>(height);
  for(png_uint_32 k = 0; k < height; ++k)
    rows[k] = png_bytep(buffer.get() + k*width*4);

  // And save the image
  saveImage(out, rows, width, height, comments);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::saveImage(const string& filename, const FBSurface& surface,
                           const GUI::Rect& rect, const VariantList& comments)
{
  ofstream out(filename, std::ios_base::binary);
  if(!out.is_open())
    throw runtime_error("ERROR: Couldn't create snapshot file");

  // Do we want the entire surface or just a section?
  png_uint_32 width = rect.width(), height = rect.height();
  if(rect.empty())
  {
    width = surface.width();
    height = surface.height();
  }

  // Get the surface pixel data (we get ABGR format)
  unique_ptr<png_byte[]> buffer = make_unique<png_byte[]>(width * height * 4);
  surface.readPixels(buffer.get(), width, rect);

  // Set up pointers into "buffer" byte array
  unique_ptr<png_bytep[]> rows = make_unique<png_bytep[]>(height);
  for(png_uint_32 k = 0; k < height; ++k)
    rows[k] = png_bytep(buffer.get() + k*width*4);

  // And save the image
  saveImage(out, rows, width, height, comments);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::saveImage(ofstream& out, const unique_ptr<png_bytep[]>& rows,
    png_uint_32 width, png_uint_32 height, const VariantList& comments)
{
  #define saveImageERROR(s) { err_message = s; goto done; }

  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  const char* err_message = nullptr;

  // Create the PNG saving context structure
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                 png_user_error, png_user_warn);
  if(png_ptr == nullptr)
    saveImageERROR("Couldn't allocate memory for PNG file");

  // Allocate/initialize the memory for image information.  REQUIRED.
	info_ptr = png_create_info_struct(png_ptr);
  if(info_ptr == nullptr)
    saveImageERROR("Couldn't create image information for PNG file");

  // Set up the output control
  png_set_write_fn(png_ptr, &out, png_write_data, png_io_flush);

  // Write PNG header info
  png_set_IHDR(png_ptr, info_ptr, width, height, 8,
      PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT);

  // Write comments
  writeComments(png_ptr, info_ptr, comments);

  // Write the file header information.  REQUIRED
  png_write_info(png_ptr, info_ptr);

  // Pack pixels into bytes
  png_set_packing(png_ptr);

  // Swap location of alpha bytes from ARGB to RGBA
  png_set_swap_alpha(png_ptr);

  // Pack ARGB into RGB
  png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);

  // Flip BGR pixels to RGB
  png_set_bgr(png_ptr);

  // Write the entire image in one go
  png_write_image(png_ptr, rows.get());

  // We're finished writing
  png_write_end(png_ptr, info_ptr);

  // Cleanup
done:
  if(png_ptr)
    png_destroy_write_struct(&png_ptr, &info_ptr);
  if(err_message)
    throw runtime_error(err_message);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool PNGLibrary::allocateStorage(png_uint_32 w, png_uint_32 h)
{
  // Create space for the entire image (3 bytes per pixel in RGB format)
  uInt32 req_buffer_size = w * h * 3;
  if(req_buffer_size > ReadInfo.buffer_size)
  {
    ReadInfo.buffer = make_unique<png_byte[]>(req_buffer_size);
    if(ReadInfo.buffer == nullptr)
      return false;

    ReadInfo.buffer_size = req_buffer_size;
  }
  uInt32 req_row_size = h;
  if(req_row_size > ReadInfo.row_size)
  {
    ReadInfo.row_pointers = make_unique<png_bytep[]>(req_row_size);
    if(ReadInfo.row_pointers == nullptr)
      return false;

    ReadInfo.row_size = req_row_size;
  }

  ReadInfo.width  = w;
  ReadInfo.height = h;
  ReadInfo.pitch  = w * 3;

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::loadImagetoSurface(FBSurface& surface)
{
  // First determine if we need to resize the surface
  uInt32 iw = ReadInfo.width, ih = ReadInfo.height;
  if(iw > surface.width() || ih > surface.height())
    surface.resize(iw, ih);

  // The source dimensions are set here; the destination dimensions are
  // set by whoever owns the surface
  surface.setSrcPos(0, 0);
  surface.setSrcSize(iw, ih);

  // Convert RGB triples into pixels and store in the surface
  uInt32 *s_buf, s_pitch;
  surface.basePtr(s_buf, s_pitch);
  uInt8* i_buf = ReadInfo.buffer.get();
  uInt32 i_pitch = ReadInfo.pitch;

  for(uInt32 irow = 0; irow < ih; ++irow, i_buf += i_pitch, s_buf += s_pitch)
  {
    uInt8*  i_ptr = i_buf;
    uInt32* s_ptr = s_buf;
    for(uInt32 icol = 0; icol < ReadInfo.width; ++icol, i_ptr += 3)
      *s_ptr++ = myFB.mapRGB(*i_ptr, *(i_ptr+1), *(i_ptr+2));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::writeComments(png_structp png_ptr, png_infop info_ptr,
                               const VariantList& comments)
{
  uInt32 numComments = uInt32(comments.size());
  if(numComments == 0)
    return;

  unique_ptr<png_text[]> text_ptr = make_unique<png_text[]>(numComments);
  for(uInt32 i = 0; i < numComments; ++i)
  {
    text_ptr[i].key = const_cast<char*>(comments[i].first.c_str());
    text_ptr[i].text = const_cast<char*>(comments[i].second.toCString());
    text_ptr[i].compression = PNG_TEXT_COMPRESSION_NONE;
    text_ptr[i].text_length = 0;
  }
  png_set_text(png_ptr, info_ptr, text_ptr.get(), numComments);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::png_read_data(png_structp ctx, png_bytep area, png_size_t size)
{
  (static_cast<ifstream*>(png_get_io_ptr(ctx)))->read(
    reinterpret_cast<char *>(area), size);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::png_write_data(png_structp ctx, png_bytep area, png_size_t size)
{
  (static_cast<ofstream*>(png_get_io_ptr(ctx)))->write(
    reinterpret_cast<const char *>(area), size);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::png_io_flush(png_structp ctx)
{
  (static_cast<ofstream*>(png_get_io_ptr(ctx)))->flush();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::png_user_warn(png_structp ctx, png_const_charp str)
{
  throw runtime_error(string("PNGLibrary warning: ") + str);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PNGLibrary::png_user_error(png_structp ctx, png_const_charp str)
{
  throw runtime_error(string("PNGLibrary error: ") + str);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PNGLibrary::ReadInfoType PNGLibrary::ReadInfo = {
  nullptr, nullptr, 0, 0, 0, 0, 0
};
