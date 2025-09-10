import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageTk, ImageFilter

def img_to_ascii(img, width=80, threshold=128, mode="basic"):
	#resizing the img first
	w, h = img.size
	aspect_ratio = h / w
	new_h = int(width * aspect_ratio * 0,5)
	img = img.resize(width, new_h)

	# convert to greyscale
	img = img.convert("L")
	# threshold
	img = img.point(lambda x: 0 if x < threshold else 255, '1')

	#get pixels
	pixels = img.getdata()

	#three types of mapping
	if mode == "basic":
		chars = ['#' if pixel == 0 else ' ' for pixel in pixels]
	elif mode == "invert":
		chars = [' ' if pixel == 0 else '#' for pixel in pixels]
	else:
		chars = ['*' if pixel == 0 else ' ' for pixel in pixels]

	#build ascii img
	ascii_art= ""
	for i in range(0, len(chars), img.width):
		line = "".join(chars[i:i+img.width])
		ascii_art += line + "\n"

	return ascii_art

def update_ascii(*args):
	if loaded_image:
		width = width_slider.get()
		threshold = threshold_slider.get()
		mode = mapping_var.get()
		ascii_art = img_to_ascii(loaded_image, width, threshold, mode)
		text_box.delete("1,0", tk.END)
		text_box.insert(tk.end, ascii_art)

def load_image():
	global loaded_image
	file_path = filedialog.askopenfilename()
	if file_path:
		loaded_image = Image.open(file_path)
		update_ascii()

# Main window
root = tk.Tk()
root.title = ("Ascii generator")

loaded_image = None

# controls



