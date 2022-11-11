import turtle


def mandelbrot_dist(cx,cy, maxticks=1000):
    f = lambda zx,zy: (zx*zx-zy*zy + cx, 2*zx*zy + cy)
    z = (0, 0)
    for tick in range(maxticks+1):
        z = f(*z)
        if (z[0]**2 + z[1]**2) > 2*2: # Is it diverging?
            break
    return tick/maxticks


def dist2colour(dist, colmap=0):
    col = int(dist * 256*256*256)
    B = col % 256
    G = (col-B)//256 % 256
    R = (((col-B)//256 - G)//256) % 256
    if (colmap==1):
        return (R//2,G//2,B//2)
    elif (colmap==2):
        return (128+R//2,128+G//2,128+B//2)
    elif (colmap==3):
        return (64+R//2,64+G//2,64+B//2)
    else:
        return (R,G,B)


def draw_set(set_function, xrange=(-100,50), yrange=(-100,100), step=5, zoom=1, x0y0=(0,0), colmap=0):
    scale = 3.0/zoom/max([max(xrange), max(yrange)])
    x0,y0 = x0y0
    turtle.colormode(255)
    turtle.width(step)
    for x in range(*xrange, step):
        turtle.penup()
        for y in range(*yrange, step):
            turtle.setpos(x+x0,y+y0)
            turtle.pendown()
            dist = set_function((x-x0)*scale, (y-y0)*scale)
            turtle.color(dist2colour(dist, colmap=colmap))
            turtle.forward(1)
        
            
if __name__ == "__main__":
    draw_set(set_function=mandelbrot_dist)