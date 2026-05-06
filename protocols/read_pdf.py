import pdfplumber
import sys
import os

# 使用 UART协议.pdf 文件
pdf_path = r"d:\Trae\OpenCPU_SDK_General\SDK\PLAT\project\ec7xx_0h00\ap\apps\lierda_app\customer_app\protocols\UART协议.pdf"
output_path = r"d:\Trae\OpenCPU_SDK_General\SDK\PLAT\project\ec7xx_0h00\ap\apps\lierda_app\customer_app\protocols\UART协议_extracted.txt"

print(f"PDF 路径：{pdf_path}")
print(f"检查文件是否存在：{pdf_path}")
print(f"文件存在：{os.path.exists(pdf_path)}")
print(f"当前工作目录：{os.getcwd()}")
print(f"脚本目录：{os.path.dirname(__file__)}")

try:
    if not os.path.exists(pdf_path):
        print("文件不存在！")
        # 列出目录内容
        print(f"\n目录内容：")
        for item in os.listdir(os.path.dirname(__file__)):
            print(f"  - {item}")
        sys.exit(1)
    
    print("\n正在使用 pdfplumber 读取 PDF 文件...\n")
    print("="*80 + "\n")
    
    # 使用 pdfplumber 读取 PDF
    full_text = ""
    with pdfplumber.open(pdf_path) as pdf:
        print(f"PDF 文件成功打开！")
        print(f"总页数：{len(pdf.pages)}")
        print("\n" + "="*80 + "\n")
        
        # 提取所有页面的文本
        for i, page in enumerate(pdf.pages):
            text = page.extract_text()
            if text:
                full_text += text
                print(f"--- 第 {i+1} 页 ---")
                print(text)
                print("\n" + "="*80 + "\n")
    
    # 保存提取的文本到文件
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(full_text)
    print(f"\n提取的文本已保存到：{output_path}")
        
except Exception as e:
    print(f"读取 PDF 文件时出错：{e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
