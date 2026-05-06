import pdfplumber
import sys
import os

pdf_path = r"D:\Trae\OpenCPU_SDK_General\SDK\PLAT\project\ec7xx_0h00\ap\apps\lierda_app\customer_app\protocols\UART 通信协议_HEX_V2.2.pdf"
output_path = r"D:\Trae\OpenCPU_SDK_General\SDK\PLAT\project\ec7xx_0h00\ap\apps\lierda_app\customer_app\protocols\hex_v2.2_extracted.txt"

print("="*80)
print("正在解析 hex.pdf...")
print("="*80)

try:
    if not os.path.exists(pdf_path):
        print("文件不存在！")
        sys.exit(1)
    
    full_text = ""
    with pdfplumber.open(pdf_path) as pdf:
        print(f"PDF 页数：{len(pdf.pages)}")
        print("="*80)
        
        for i, page in enumerate(pdf.pages):
            text = page.extract_text()
            if text:
                full_text += text + "\n"
                print(f"\n--- 第 {i+1} 页 ---")
                print(text)
                print("="*80)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(full_text)
    
    print(f"\n提取的文本已保存到：{output_path}")
    
except Exception as e:
    print(f"读取 PDF 文件时出错：{e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
