.SILENT:

build:
	gcc -o shaders shaders.c -O2 -lm -lpthread
	echo "Built shaders exectuable..."
	mkdir -p output
	echo "Generating shader frames (may take a while)..."
	./shaders
	echo "Compiling output.mp4..."
	ffmpeg -y -loglevel error -framerate 60 -i output/output-%03d.ppm -c:v libx264 -pix_fmt yuv420p output/output.mp4
	echo "Cleaning up shader frames..."
	rm output/*.ppm
	echo "Generated output successfully!"

