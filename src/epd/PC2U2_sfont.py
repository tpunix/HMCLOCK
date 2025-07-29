import re
import struct

# 读取 font.txt 中的全部内容
with open("font.txt", "r", encoding="gbk", errors="ignore") as f:
    lines = f.readlines()

# 用于存储字体数据
font_chars = []

# 临时缓存 bitmap 和字符信息
current_bitmap = []
current_char = ""
width = height = 0

# 正则匹配
bitmap_line_re = re.compile(r'\{([0-9xA-Fa-f, ]+)\}')
char_comment_re = re.compile(r'/\*\s*"(.+?)",\s*(\d+)\s*\*/')
size_comment_re = re.compile(r'/\*\s*\(\s*(\d+)\s*X\s*(\d+)\s*,.*\*/')

for line in lines:
    # 如果是bitmap行
    bitmap_match = bitmap_line_re.search(line)
    if bitmap_match:
        hex_str = bitmap_match.group(1)
        bytes_list = [int(x, 16) for x in hex_str.split(',')]
        current_bitmap.extend(bytes_list)
    
    # 如果是字符注释行
    char_match = char_comment_re.search(line)
    if char_match:
        current_char = char_match.group(1)
    
    # 如果是尺寸注释行
    size_match = size_comment_re.search(line)
    if size_match:
        width = int(size_match.group(1))
        height = int(size_match.group(2))

        # 生成 FontChar 结构
        ucs = ord(current_char)
        ft_adv = width
        ft_bw = width
        ft_bh = height
        ft_bx = 0
        ft_by = 0

        header = [ft_adv, ft_bw, ft_bh, ft_bx, ft_by]

        font_chars.append({
            "char": current_char,
            "ucs": ucs,
            "header": header,
            "bitmap": current_bitmap.copy()
        })

        # 清空临时变量
        current_bitmap.clear()
        current_char = ""
        width = height = 0

# 构造 sfont[] 数据
font_count = len(font_chars)
offset = 2 + font_count * 4  # 2 字节总数 + 每个字符 4 字节索引
index_entries = []
data_blocks = []

for fc in font_chars:
    # UCS + 偏移
    index_entries.append(struct.pack('<HH', fc["ucs"], offset))

    # 头 + bitmap
    header = bytes(fc["header"])
    bitmap = bytes(fc["bitmap"])
    data_block = header + bitmap
    data_blocks.append(data_block)

    offset += len(data_block)

# 最终构建 sfont 数组
sfont_data = struct.pack('<H', font_count) + b''.join(index_entries) + b''.join(data_blocks)

# 输出为C数组
def format_c_array(data, name="sfont"):
    lines = [f'static unsigned char {name}[] __attribute__((aligned(4))) = {{']
    for i in range(0, len(data), 12):
        chunk = ','.join(f'0x{b:02X}' for b in data[i:i+12])
        lines.append(f'  {chunk},')
    lines.append('};')
    return '\n'.join(lines)

# 保存为sfont.h
with open("sfont.h", "w", encoding="utf-8") as f:
    f.write("#ifndef __sfont__\n#define __sfont__\n\n")
    f.write(f"#define sfont_size {len(sfont_data)}\n\n")
    f.write(format_c_array(sfont_data))
    f.write("\n\n#endif // __sfont__\n")
