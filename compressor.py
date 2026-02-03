import subprocess
from logger import log_message, LogLevel

from file_handler import copy_file, log_message, LogLevel
from PIL import Image
import pillow_heif

PHOTO_QUALITY = 50
COMPRESS_LEVEL = 6

VIDEO_QUALITY = '28' #20-23 = good, 24–28 = noticable quality loss, 30+ poor

def compress_image(file, output_file, extension):
    """
    Compresses an image
    Parameters:
        file (str): Path to the input image.
        output_file (str): Path to the output compressed image.
        extension (str): File extension (e.g., '.jpg', '.png', '.heic').
    Returns:
        bool: True if compression succeeds.
    """
    if not file or not output_file or not extension:
        log_message(LogLevel.LOG_ERROR, "Null input or output path in compress_image")
        return False

    ext_lower = extension.lower()
    pillow_heif.register_heif_opener()

    try:
        with Image.open(file) as img:
            save_args = {}

            if ext_lower in [".jpg", ".jpeg"]:
                save_args = {
                    "format": "JPEG",
                    "quality": PHOTO_QUALITY,
                    "optimize": True,
                    "exif": img.info.get("exif")
                }
            elif ext_lower == ".png":
                save_args = {
                    "format": "PNG",
                    "optimize": True,
                    "compress_level": COMPRESS_LEVEL
                }
            elif ext_lower == ".heic":
                save_args = {
                    "format": "HEIF",
                    "quality": PHOTO_QUALITY
                }
            else:
                log_message(LogLevel.LOG_WARNING, f"{extension} extension not supported for compression. Copying raw file: {file}")
                if not copy_file(file, output_file):
                    log_message(LogLevel.LOG_ERROR, f"Failed to copy {file} to {output_file}")
                    return False
                return True

            img.save(output_file, **save_args)
        return True

    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to compress {file} to {output_file}: {e}")
        return False

def compress_video(file, output_file):
    """
    Compresses a video using FFmpeg.
    Parameters:
        file (str): Path to the input video file.
        output_file (str): Path to the output compressed video file.
    Returns:
        bool: True if compression succeeds.
    """
    cmd = [
        'ffmpeg',
        '-i', file,
        '-c:v', 'libx264',
        '-crf', VIDEO_QUALITY,
        '-preset', 'medium',
        '-c:a', 'aac', #or copy?
        '-y',
        output_file
    ]
    try:
        subprocess.run(cmd, capture_output=True, text=True, check=True)
        log_message(LogLevel.LOG_INFO, f"Successfully compressed video: {file}")
        return True
    except subprocess.CalledProcessError as e:
        log_message(LogLevel.LOG_ERROR, f"FFmpeg error for {file}: {e.stderr}")
        return False
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to compress video {file}: {e}")
        return False