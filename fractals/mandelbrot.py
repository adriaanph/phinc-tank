import matplotlib.pyplot as plt
import matplotlib.animation as anm
import numpy as np


def mandelbrot_orbit(cx,cy, steps=10):
    """ @return: [z_0, z_1, ... z_N] with each z like (cx, cy) """
    f = lambda zx,zy: (zx*zx-zy*zy + cx, 2*zx*zy + cy) # z*z + c
    z = (0, 0)
    o = []
    for step in range(0,steps,1):
        z = f(z[0], z[1])
        o.append(z)
    return o

def mandelbrot_set(cx,cy, max_iterations=200):
    """ @return: score (same shape as cx & cy), ~0 if it converges to the set, ~1 if it diverges """
    f = lambda zx,zy: (zx*zx-zy*zy + cx, 2*zx*zy + cy) # z*z + c
    z = (0, 0)
    d = np.zeros_like(cx)
    for tick in range(0,max_iterations,1):
        z = f(z[0], z[1])
        d[ (z[0]**2 + z[1]**2) < 2*2 ] = tick # If > 4 it is definitely diverging
    d = 1 - d/max_iterations
    return d

def julia_set(zx,zy, cx=0, cy=0, max_iterations=200):
    """ @return: score (same shape as zx & zy), ~0 if it converges to the set, ~1 if it diverges """
    R = np.sqrt(2 + np.sqrt(cx**2 + cy**2)) # TODO: solve R**2 - R >= sqrt(cx**2+cy**2)
    f = lambda zx,zy: (zx*zx-zy*zy + cx, 2*zx*zy + cy) # z*z + c
    z = (zx, zy)
    d = np.zeros_like(zx)
    for tick in range(0,max_iterations,1):
        z = f(z[0], z[1])
        d[ (z[0]**2 + z[1]**2) < R**2 ] = tick # If > R*R it is definitely diverging
    d = 1 - d/max_iterations
    return d


def draw_set(set_function, xrange=(-2,2), yrange=(-2,2), points=512, cmap=None,
             set_kwargs={}, orbit_function=None):
    """ Creates an interactive figure.
        After this you still need plt.show(block=True) to wait until it is destroyed by the user! """
    _kwargs = set_kwargs() if callable(set_kwargs) else set_kwargs
    def calc_map(xrange, yrange):
        xx, yy = np.meshgrid(np.linspace(*xrange, points), np.linspace(*yrange, points))
        return set_function(xx, yy, **_kwargs)
    
    fig, axis = plt.subplots(1, 1)
    axis._cmaps = list(plt.colormaps())
    if (cmap is not None):
        axis._cmaps.insert(0, cmap)
    score = calc_map(xrange, yrange)
    img = axis.imshow(score, origin='lower', extent=list(xrange)+list(yrange), cmap=axis._cmaps[0], interpolation='antialiased')
        
    # Change colmap when user presses 'm' and 'M' keys
    def on_keyboard(event):
        if (event.key in ['m','M']):
            if (event.key == 'm'):
                axis._cmaps.append(axis._cmaps.pop(0)) # Move current first to back
            else:
                axis._cmaps.insert(0, axis._cmaps.pop(-1)) # Move current last to front
            img.set_cmap(axis._cmaps[0])
            print("INFO: Switched cmap to %s"%axis._cmaps[0])
            event.canvas.draw()
    fig.canvas.mpl_connect('key_press_event', on_keyboard)
    
    # Draw orbits interactive starting from mouse position
    if (orbit_function is not None):
        axis.plot([np.nan], [np.nan], 'o-', color='k', linewidth=1, markersize=3, markerfacecolor='none') # An invisible line to define formatting and avoid auto limits on first orbit - which may blow up
        def on_move(event):
            x, y = event.xdata, event.ydata
            if (x is not None) and (y is not None):
                ox,oy = np.stack(orbit_function(x, y, 50), axis=-1)
                axis.lines[0].set_data(ox, oy) # Set orbit lines, without auto rescale
                event.canvas.draw()
        fig.canvas.mpl_connect('motion_notify_event', on_move)
    
    # On zoom, recalculate map to keep resolution high
    axis._busy_zooming = False
    def on_zoom(axis):
        if not axis._busy_zooming: # Guard against imshow() causing an infinite loop
            axis._busy_zooming = True
            xrange, yrange = axis.get_xlim(), axis.get_ylim()
            score = calc_map(xrange, yrange)
            img.set_data(score); img.set_extent(list(xrange)+list(yrange))
            axis._busy_zooming = False
    axis.callbacks.connect('ylim_changed', on_zoom) # Zoom first adjusts xlim, then ylim, so only trigger on this event
    
    # Animate the set through set_kwargs()
    if callable(set_kwargs):
        def on_animate(step):
            _kwargs.update(set_kwargs(step))
            score = calc_map(axis.get_xlim(), axis.get_ylim())
            img.set_data(score)
            return [img] # Return what's changed, so it can be re-drawn
        fig._animation = anm.FuncAnimation(fig, on_animate, interval=100) # MUST keep the return value alive; interval in millisec


def draw_mj(xrange=(-2,2), yrange=(-2,2), points=512, cmap=None, orbit_function=None):
    """ Plots Mandelbrot set as well as the Julia set for the coordinate under the mouse pointer.
        After this you still need plt.show(block=True) to wait until it is destroyed by the user! """
    def calc_map(xrange, yrange, set_function, set_kwargs={}):
        xx, yy = np.meshgrid(np.linspace(*xrange, points), np.linspace(*yrange, points))
        return set_function(xx, yy, **set_kwargs)
    
    fig, (ax_m, ax_j) = plt.subplots(1, 2)
    score = calc_map(xrange, yrange, mandelbrot_set)
    ax_m.imshow(score, origin='lower', extent=list(xrange)+list(yrange), cmap=cmap, interpolation='antialiased')
    ax_m.set_title("Mandelbrot")
    score = calc_map(xrange, yrange, julia_set, {"cx":0, "cy":0})
    img_j = ax_j.imshow(score, origin='lower', extent=list(xrange)+list(yrange), cmap=cmap, interpolation='antialiased')

    # Update Julia interactively from mouse position
    def on_move(event):
        x, y = event.xdata, event.ydata
        if (x is not None) and (y is not None) and (event.inaxes == ax_m):
            ax_j.set_title("Julia @ %.3f, %.3f"%(x,y))
            score = calc_map(xrange, yrange, julia_set, {"cx":x, "cy":y})
            img_j.set_data(score); img_j.set_extent(list(xrange)+list(yrange))
            event.canvas.draw()
    fig.canvas.mpl_connect('motion_notify_event', on_move)


if __name__ == "__main__":
    if False:
        draw_mj(xrange=(-3,2), yrange=(-2,2), points=256, cmap='turbo_r')
    
    elif True:
        draw_set(set_function=mandelbrot_set, orbit_function=mandelbrot_orbit, xrange=(-3,1), yrange=(-2,2), cmap='jet')
        
        def funky_julia(step=0):
            steps_per_cycle = 300 # "a" goes through 0..2pi in a cycle; 300+ steps looks smooth-ish
            a = 2*np.pi*step/steps_per_cycle
            cx, cy = 0.7885*np.cos(a), 0.7885*np.sin(a)
            return {"cx":cx, "cy":cy}
        draw_set(set_function=julia_set, set_kwargs=funky_julia, points=256, xrange=(-2,2), yrange=(-2,2), cmap='turbo_r')
        # draw_set(set_function=julia_set, set_kwargs={{"cx":0.285, "cy":0.01}}, points=1024, xrange=(-2,2), yrange=(-2,2), cmap='twilight_shifted')

    plt.show(block=True) # Interactive till the figure is closed. Need block=True for animation!
