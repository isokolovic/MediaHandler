import os
import shutil
from PIL import Image
from pillow_heif import register_heif_opener
import subprocess

register_heif_opener()

source_folder = os.curdir
destination_folder = "D:\\"

image_extensions = ["jpg", "jpeg", "heic", "png"]
video_extensions = ["mp4", "avi", "mov"]
gif_extensions = ['gif', 'txt']
photo_trim_size = (3840, 2160)
photo_quality = 70

def compress_file(file):    
    #Photo
    if (any(file.lower().endswith(extension.lower()) for extension in image_extensions)):
        image = Image.open(file)
        image.thumbnail(photo_trim_size, Image.LANCZOS) #Image.ANTIALIAS?
        image.save(file, 'JPEG', quality = photo_quality)

    #Video
    if (any(file.lower().endswith(extension.lower()) for extension in video_extensions)):
        file_copy = os.path.splitext(file)[0] + "_copy" + os.path.splitext(file)[1]

        subprocess.run('ffmpeg -i ' + file +  ' -vcodec h265 ' + file_copy) #h264?

        new_name = file
        os.remove(file)
        os.rename(file_copy, new_name)

    #Gif
    #if (any(file.lower().endswith(extension.lower()) for extension in gif_extensions)):

def run_media_migration(source_folder):    
    for root, dirs, files in os.walk(source_folder):    
        for file in files:
            if (any(file.lower().endswith(extension.lower()) for extension in image_extensions + video_extensions + gif_extensions)):
                if not os.path.exists(os.path.join(destination_folder, root)):
                    temp_dest_folder = os.mkdir(os.path.join(destination_folder, root))

                temp_dest_folder = os.path.join(destination_folder, root)

                #If file name contains blank spaces, remove them
                file.replace(" ", "")

                shutil.copyfile(os.path.join(root, file), os.path.join(temp_dest_folder, file))
                copied_file = os.path.join(temp_dest_folder, file)

                try:
                    compress_file(copied_file)
                except:
                    print(file + " NOT compressed!")
                    continue
                
run_media_migration(source_folder)