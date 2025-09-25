#!/usr/bin/env python3
import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageTk, ImageFilter

def img_to_ascii(img, width=80, threshold=128, mode="basic", density=5):
	#resizing the img first
	w, h = img.size
	aspect_ratio = h / w
	new_h = int(width * aspect_ratio * 0.5)
	img = img.resize((width, new_h))

	# convert to greyscale
	img_gray = img.convert("L")
	# threshold
	img_bw = img_gray.point(lambda x: 0 if x < threshold else 255, '1')

	# edge detection
	edge_img = img_gray.filter(ImageFilter.FIND_EDGES)
	edge_pixels = list(edge_img.getdata())
	gray_pixels = list(img_gray.getdata())
	bw_pixels = list(img_bw.getdata())

	# density and thickness character sets
	density_c = (
    " .:-=+*#%@"          # ASCII base
    "✰"        # Stars (light / airy)
    "❋"
    "𝄞𝄱"
    "⠁⠂⠄⡀⢀⠐⠠⡁⡂⡄⡈⡐⡠"  # Braille (fine-grain shading)
    "█▓▒░"                # Full/half blocks (darkest)
)

	density_c = density_c[::-1][:max(2, density)]
	edge_c = [
    "─", "│", "┌", "┐", "└", "┘", "├", "┤", "┬", "┴", "┼",
    "/", "|", "-", "\\"]

	# mapping (basic and invert)
	chars = []
	if mode == "basic":
		for i, edge_val in enumerate(edge_pixels):
			# use threshold to decide if edge is strong enough
			if edge_val > threshold:
				idx = int((i // img.width) % len(edge_c))
				chars.append(edge_c[idx])
			else:
				# use img_bw to decide if pixel is "on" or "off"
				if bw_pixels[i] == 0:
					chars.append(density_c[0])
				else:
					idx = int(gray_pixels[i] / 255 * (len(density_c) - 1))
					chars.append(density_c[idx])
	elif mode == "invert":
		density_c_inv = density_c[::-1]
		edge_c_inv = edge_c[::-1]
		for i, edge_val in enumerate(edge_pixels):
			if edge_val > threshold:
				idx = int((i // img.width) % len(edge_c_inv))
				chars.append(edge_c_inv[idx])
			else:
				if bw_pixels[i] == 0:
					chars.append(density_c_inv[0])
				else:
					idx = int(gray_pixels[i] / 255 * (len(density_c_inv) - 1))
					chars.append(density_c_inv[idx])

	# build ascii img
	ascii_art= ""
	for i in range(0, len(chars), img.width):
		line = "".join(chars[i:i+img.width])
		ascii_art += line + "\n"

	return ascii_art

def update_ascii(*args):
	if loaded_image:
		width = width_slider.get()
		threshold = threshold_slider.get()
		density = density_slider.get()
		mode = mapping_var.get()
		ascii_art = img_to_ascii(loaded_image, width, threshold, mode, density)
		text_box.delete("1.0", tk.END)
		text_box.insert(tk.END, ascii_art)
		text_box.config(bg=bg_color_var.get(), fg=fg_color_var.get())

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
control_frame = tk.Frame(root)
control_frame.pack(side=tk.TOP, fill=tk.X)

# width slider
width_slider = tk.Scale(control_frame, from_=40, to=600, orient="horizontal", label="Width")
width_slider.set(80)
width_slider.pack(side=tk.LEFT)

# threshold slide bar
threshold_slider = tk.Scale(control_frame, from_=0, to=255, orient="horizontal", label="Threshold")
threshold_slider.set=(128)
threshold_slider.pack(side=tk.LEFT)

#density slide bar
density_slider = tk.Scale(control_frame, from_=2, to=100, orient="horizontal", label="Density")
density_slider.set=(5)
density_slider.pack(side=tk.LEFT)

# mapping mode
mapping_var = tk.StringVar(value="basic")
mapping_menu = tk.OptionMenu(control_frame, mapping_var, "basic", "invert")
mapping_menu.pack(side=tk.LEFT)
mapping_menu_label = tk.Label(control_frame, text="Mapping")
mapping_menu_label.pack(side=tk.LEFT)

#color options
color_options = ["white", "black", "cyan", "magenta"]

bg_color_var = tk.StringVar(value="white")
bg_color_menu = tk.OptionMenu(control_frame, bg_color_var, "white", "black")
bg_color_menu.pack(side=tk.LEFT)
bg_color_menu_label = tk.Label(control_frame, text="Background")
bg_color_menu_label.pack(side=tk.LEFT)

fg_color_var = tk.StringVar(value="black")
fg_color_menu = tk.OptionMenu(control_frame, fg_color_var, "white", "black", "cyan", "magenta")
fg_color_menu.pack(side=tk.LEFT)
fg_color_menu_label = tk.Label(control_frame, text="Foreground")
fg_color_menu_label.pack(side=tk.LEFT)

# load image button
load_button = tk.Button(control_frame, text="Load Image", command=load_image)
load_button.pack(side=tk.LEFT)

# ascii output
text_box = tk.Text(root, font=("Courier", 10), bg="white", fg="black")
text_box.pack(fill=tk.BOTH, expand=True)

# update ascii when parameters change in sliders
width_slider.bind("<Motion>", update_ascii)
threshold_slider.bind("<Motion>", update_ascii)
density_slider.bind("<Motion>", update_ascii)
mapping_var.trace_add("write", update_ascii)
bg_color_var.trace_add("write", update_ascii)
fg_color_var.trace_add("write", update_ascii)

root.mainloop()
