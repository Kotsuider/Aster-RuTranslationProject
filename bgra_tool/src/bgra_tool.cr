require "stumpy_png"

# ─────────────────────────────────────────────────────────────────────────────
# G2 Engine BGRA format
#
# Header (16 bytes, all little-endian):
#   0x00  UInt32  Signature  0x41524742 ('BGRA')
#   0x04  UInt32  Bitmap     0x08080808 (8 bits per channel × 4)
#   0x08  UInt32  Width
#   0x0C  UInt32  Height
#   0x10  …       Raw pixels, BGRA order, 8 bits per channel
# ─────────────────────────────────────────────────────────────────────────────

BGRA_SIGNATURE = 0x41524742_u32 # 'BGRA' in little-endian
BGRA_BITMAP    = 0x08080808_u32

DECODE_EXTS = {".argb", ".arg"}
ENCODE_EXTS = {".png"}

# ── Decode: .argb / .arg  →  .png ────────────────────────────────────────────

def decode(input : String, output : String) : Nil
  File.open(input, "rb") do |f|
    sig = f.read_bytes(UInt32, IO::ByteFormat::LittleEndian)
    raise "Not a BGRA file (bad signature): #{input}" unless sig == BGRA_SIGNATURE

    bitmap = f.read_bytes(UInt32, IO::ByteFormat::LittleEndian)
    raise "Unsupported bitmap mask 0x#{bitmap.to_s(16)}: #{input}" unless bitmap == BGRA_BITMAP

    width  = f.read_bytes(UInt32, IO::ByteFormat::LittleEndian).to_i32
    height = f.read_bytes(UInt32, IO::ByteFormat::LittleEndian).to_i32

    raise "Invalid dimensions #{width}×#{height}" if width <= 0 || height <= 0

    raw = Bytes.new(width * height * 4)
    f.read_fully(raw)

    canvas = StumpyPNG::Canvas.new(width, height)

    height.times do |y|
      width.times do |x|
        idx = (y * width + x) * 4
        b = raw[idx].to_u16
        g = raw[idx + 1].to_u16
        r = raw[idx + 2].to_u16
        a = raw[idx + 3].to_u16
        # stumpy_png stores 16-bit channels; scale 8-bit → 16-bit with ×257
        # (255 × 257 = 65535 = 0xFFFF, preserves full range)
        canvas[x, y] = StumpyCore::RGBA.new(r * 257_u16, g * 257_u16, b * 257_u16, a * 257_u16)
      end
    end

    StumpyPNG.write(canvas, output)
  end
  puts "  decoded  #{input}  →  #{output}  (#{StumpyPNG.read(output).width}×#{StumpyPNG.read(output).height})"
rescue ex
  STDERR.puts "ERROR decoding #{input}: #{ex.message}"
end

# ── Encode: .png  →  .argb ───────────────────────────────────────────────────

def encode(input : String, output : String) : Nil
  canvas = StumpyPNG.read(input)

  File.open(output, "wb") do |f|
    f.write_bytes(BGRA_SIGNATURE, IO::ByteFormat::LittleEndian)
    f.write_bytes(BGRA_BITMAP,    IO::ByteFormat::LittleEndian)
    f.write_bytes(canvas.width.to_u32,  IO::ByteFormat::LittleEndian)
    f.write_bytes(canvas.height.to_u32, IO::ByteFormat::LittleEndian)

    canvas.height.times do |y|
      canvas.width.times do |x|
        rgba = canvas[x, y]
        # Convert 16-bit → 8-bit by taking the high byte (>> 8)
        f.write_byte((rgba.b >> 8).to_u8)
        f.write_byte((rgba.g >> 8).to_u8)
        f.write_byte((rgba.r >> 8).to_u8)
        f.write_byte((rgba.a >> 8).to_u8)
      end
    end
  end
  puts "  encoded  #{input}  →  #{output}  (#{canvas.width}×#{canvas.height})"
rescue ex
  STDERR.puts "ERROR encoding #{input}: #{ex.message}"
end

# ── Route a single file by extension ─────────────────────────────────────────

def process_file(path : String) : Nil
  ext = File.extname(path).downcase

  if DECODE_EXTS.includes?(ext)
    output = path.rchop(ext) + ".png"
    decode(path, output)
  elsif ENCODE_EXTS.includes?(ext)
    output = path.rchop(ext) + ".argb"
    encode(path, output)
  else
    STDERR.puts "Skipped (unknown extension): #{path}"
  end
end

# ── Walk a directory (non-recursive, top-level only) ─────────────────────────

def process_dir(dir : String) : Nil
  all_exts = DECODE_EXTS.to_a + ENCODE_EXTS.to_a
  found = 0

  Dir.each_child(dir) do |name|
    ext = File.extname(name).downcase
    next unless all_exts.includes?(ext)

    process_file(File.join(dir, name))
    found += 1
  end

  puts "Done. #{found} file(s) processed in #{dir}." if found == 0
end

# ── Entry point ───────────────────────────────────────────────────────────────

if ARGV.empty?
  STDERR.puts <<-USAGE
    Usage: bgra_tool <file_or_directory> [file_or_directory ...]

    Conversions performed automatically by extension:
      .argb / .arg  →  .png   (decode)
      .png          →  .argb  (encode)

    Examples:
      bgra_tool image.argb
      bgra_tool sprite.png
      bgra_tool ./assets/
  USAGE
  exit 1
end

ARGV.each do |path|
  unless File.exists?(path) || Dir.exists?(path)
    STDERR.puts "Not found: #{path}"
    next
  end

  if Dir.exists?(path)
    puts "Directory: #{path}"
    process_dir(path)
  else
    process_file(path)
  end
end