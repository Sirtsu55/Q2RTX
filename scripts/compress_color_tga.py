# This python file will compress all the textures that are found in baseq2
# If requests module is not installed, run the following command:
# pip install requests

import glob
import os
from zipfile import ZipFile
import time
import shutil

def path(path):
    if(os.name == "nt"):
        return os.path.abspath(path).replace("/", "\\")
    else:
        return os.path.abspath(path).replace("\\", "/")
    

data_folder = path("../basenac")

recurse_path_tga = path(f"{data_folder}/**/*.tga")
recurse_path_mat = path(f"{data_folder}/**/*.mat")

compressonator_version = "4.4.19"
os_name = "win64" # win64, amd64, Linux; Only win64 is tested

compressonator_url = f"https://github.com/GPUOpen-Tools/compressonator/releases/download/V{compressonator_version}/compressonatorcli-{compressonator_version}-{os_name}.zip"
tools_folder = path("../tools/")
compressonator_folder_name = f"compressonatorcli-{compressonator_version}-{os_name}"
compressonator_cli = path(f"{tools_folder}/{compressonator_folder_name}/compressonatorcli.exe")

def extract_zip(zip_path, extract_path):
    with ZipFile(zip_path, mode="r") as zf:
        zf.extractall(extract_path)
    os.remove(zip_path)
if(os.path.exists(compressonator_cli)):
    print("Compressonator is already installed")
else:
    print("Compresonator is not installed, downloading now...")

    os.system(f"curl -L {compressonator_url} -o compressonator.zip")

    extract_zip("compressonator.zip", tools_folder)


compressed_folder = path(f"{data_folder}/compressed/")

if(not os.path.exists(compressed_folder)):
    os.mkdir(compressed_folder)

color_textures = set()
emissive_textures = set()

for file in glob.iglob(recurse_path_tga, recursive=True):
    full_path = path(file)

    if(file.endswith("_n.tga") or file.endswith("_light.tga")):
        continue
    else:
        color_textures.add(full_path)

for file in glob.iglob(recurse_path_mat, recursive=True):
    full_path = path(file)
    with open(full_path, "r") as f:
        for line in f:
            line = line.strip()
            args = line.split(" ")
            if(args[0] == "texture_emissive"):
                emissive_textures.add(path(f"{data_folder}/{args[1]}"))


compressonator_args = "-fd BC7 -EncodeWith GPU -mipsize 16 -silent"

try:
    for texture in color_textures:
        print("=====================================")
        print(f"Compressing {texture}")

        if(texture in emissive_textures):
            print("Emissive texture detected, skipping...")
            continue

        output_path = path(f"{compressed_folder}/{os.path.basename(texture).replace('.tga', '.dds')}")

        if(os.path.exists(output_path)):
            print("File already exists, skipping...")
            continue

        command = f"{compressonator_cli} {compressonator_args} {texture} {output_path}"

        t1 = time.time()
        os.system(command)
        t2 = time.time()
        
        print(f"Outputted to {output_path}")
        print(f"Time elapsed: {t2 - t1} seconds")
    
except KeyboardInterrupt:
    print("Keyboard interrupt detected, exiting...")

input("Press enter to exit...")

