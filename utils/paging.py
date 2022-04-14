
page_size = 4096
base_addr = 0xFE000002
size = 10 
print("Requested Physical Memory:")
print("==========================")
print(f"Address: {base_addr:x}")
print(f"Size: {size}")

offset = base_addr % page_size
print(f"Offset: {offset}")
base_addr = base_addr - offset

print("Mapped Physical Memory:")
print("======================")
print(f"Address: {base_addr:x}")
size = size + offset
print(f"Size: {size}")

print(f"Received ptr + {offset}")