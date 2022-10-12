import os
import shutil
from PIL import Image
from pillow_heif import register_heif_opener
import subprocess

register_heif_opener()

source_folder = os.curdir
destination_folder = "D:\\"

image_extensions = ["jpg", "jpeg", "heic", "png", 'bmp']
video_extensions = ["mp4", "avi", "mov", "3gp"]
gif_extensions = ["gif", "txt", "mp3"]
photo_trim_size = (3840, 2160)
photo_quality = 70
special_chars = " %:/,\{}~[]<>*?čćžđšČĆŽŠĐ"

# Handles compression according to media file type
def compress_file(file):
    # Photo
    if (any(file.lower().endswith(extension.lower()) for extension in image_extensions)):
        image = Image.open(file)
        image.thumbnail(photo_trim_size, Image.LANCZOS) #Resampling.LANCZOS?
        image.save(file, 'JPEG', quality = photo_quality)

    # Video
    if (any(file.lower().endswith(extension.lower()) for extension in video_extensions)):
        file_copy = os.path.splitext(file)[0] + "_copy" + os.path.splitext(file)[1]

        subprocess.run('ffmpeg -i ' + file +  ' -vcodec h264 ' + file_copy) #h264?

        # ffmpeg creates a copy -> rename the copy to original name and delete non-copressed
        new_name = file
        os.remove(file)
        os.rename(file_copy, new_name)

    # Gif
    if (any(file.lower().endswith(extension.lower()) for extension in gif_extensions)):
        pass

# Checks if file type is of media type
def is_file_type_valid(file):
    if(any(file.lower().endswith(extension.lower()) for extension in image_extensions + video_extensions + gif_extensions)):
        return True
    return False

# Remove special characters from file or folder name
def clean_name(element):
    if not(os.path.isdir(element)):
        return ''.join([char for char in element if char not in special_chars])
    return (''.join([char for char in element if char not in special_chars])).replace('.', '')

# Create folder structure, copy and copress files
def create_folder_and_copy_file_if_needed(root, file):
    # Create folder if needed
    if not os.path.exists(os.path.join(destination_folder, clean_name(root))):
        temp_dest_folder = os.mkdir(os.path.join(destination_folder, clean_name(root)))                    
    temp_dest_folder = os.path.join(destination_folder, clean_name(root))

    # Copy file to destination location if needed 
    if not clean_name(file) in os.listdir(temp_dest_folder):
        shutil.copyfile(os.path.join(root, file), os.path.join(temp_dest_folder, clean_name(file)))
    
    copied_file = os.path.join(temp_dest_folder, clean_name(file))

    # Compress file if needed
    try:
        compress_file(copied_file)
    except:
        print(file + " NOT compressed!")
    
    # If compression failed (compressed file is not bigger than original), replace compressed file with original
    if file in os.listdir(temp_dest_folder) and os.path.getsize(copied_file) > os.path.getsize(os.path.join(root, file)):
        os.remove(copied_file)
        shutil.copyfile(os.path.join(root, file), os.path.join(temp_dest_folder, clean_name(file)))

# Main method
def run_media_migration(source_folder):    
    for root, dirs, files in os.walk(source_folder):
        for file in files:
            if (is_file_type_valid(file)):
                print(file)

                create_folder_and_copy_file_if_needed(root, file)
                
if __name__ == "__main__":
    run_media_migration(source_folder)