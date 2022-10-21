import os


ret = os.system("./cap")

#print(ret)

#f = open("IMG_0004.pgm",'r')

#print(img)
#from pgm_reader import Reader

f = 'IMG_0004.pgm'

def read_pgm(pgmf):
	
	header = pgmf.readline()
 	assert header[: 2] ==  'P5'
	(width, height) = [int(i) for i in header.split()[1: 3]]
	depth = int(header.split()[3])
	assert depth <= 65535

	raster = []
	for y in range(height):
	   row = []
		for y in range(width):
			low_bits = ord(pgmf.read(1))
			row.append(low_bits + 255 * ord(pgmf.read(1)))
		raster.append(row)
	return raster

f = open(f, 'rb')
im = read_pgm(f)
f.close()
im = np.array(im)
