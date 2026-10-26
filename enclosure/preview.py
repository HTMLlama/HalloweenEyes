"""Preview renders for eyes_frame.py (matplotlib, no GPU needed)."""
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import LightSource
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def _pv(m):
    import pyvista as pv
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    t = np.asarray(mesh.tri_verts)
    faces = np.hstack([np.full((len(t), 1), 3), t]).ravel()
    return pv.PolyData(v, faces)


def _dummies(E):
    """Stand-in parts so the renders show what goes where."""
    from manifold3d import Manifold

    def box(x0, x1, y0, y1, z0, z1):
        return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))

    def cyl(r, z0, z1, x, y):
        return Manifold.cylinder(z1 - z0, r, r, 64).translate((x, y, z0))

    glass, sclera, iris, pupil = [], [], [], []
    for px in E.POD_XS:
        wx = px + E.OLED_WINDOW_DX
        glass.append(box(px - 17, px + 17, E.EYE_Y - 17, E.EYE_Y + 17, E.WALL + 0.3, E.WALL + 2.0))
        glass.append(box(px - 18.5, px + 18.5, E.EYE_Y - 22.25, E.EYE_Y + 22.25,
                         E.WALL + E.OLED_GLASS_T, E.WALL + E.OLED_GLASS_T + 1.6))
        sclera.append(box(wx - 13.43, wx + 13.43, E.EYE_Y - 13.43, E.EYE_Y + 13.43, E.WALL + 0.25, E.WALL + 0.3))
        iris.append(cyl(8.5, E.WALL + 0.2, E.WALL + 0.25, wx, E.EYE_Y))
        pupil.append(box(wx - 1.6, wx + 1.6, E.EYE_Y - 7, E.EYE_Y + 7, E.WALL + 0.1, E.WALL + 0.2))
    jz = E.JOY_PCB_Z
    joy = [box(-13, 13, E.JOY_Y - 17, E.JOY_Y + 17, jz - 1.6, jz),
           Manifold.sphere(12.5, 64).translate((0, E.JOY_Y, jz + 10)),
           cyl(3, jz + 10, E.DEPTH + 9, 0, E.JOY_Y),
           cyl(11, E.DEPTH + 7, E.DEPTH + 12, 0, E.JOY_Y)]
    fx0 = E.FEATHER_X_RIGHT - E.FEATHER_W
    feather = [box(fx0, E.FEATHER_X_RIGHT, E.FEATHER_Y0, E.FEATHER_Y0 + E.FEATHER_L, E.FEATHER_Z0, E.FEATHER_Z0 + 1.6)]
    batt = [box(-18, 18, E.BATT_Y0 + 0.5, E.BATT_Y0 + 65.5, E.WALL, E.WALL + 10)]
    u = Manifold.batch_boolean
    from manifold3d import OpType
    return {
        "glass": u(glass, OpType.Add), "sclera": u(sclera, OpType.Add), "iris": u(iris, OpType.Add), "pupil": u(pupil, OpType.Add),
        "joy": u(joy, OpType.Add), "feather": u(feather, OpType.Add), "batt": u(batt, OpType.Add),
    }


def render(front, back, out_dir, E=None):
    import pyvista as pv
    if E is None:
        import eyes_frame as E
    d = _dummies(E)
    orange, black = "#e8741c", "#2a2a2e"
    cols = {"glass": "#101014", "sclera": "#e9e2d0", "iris": "#f2c21b", "pupil": "#000000", "joy": "#1a1a1a",
            "feather": "#2b2b2b", "batt": "#c9ccd1"}
    shots = []

    def shot(name, items, cam, up=(0, 1, 0), size=(900, 1200)):
        p = pv.Plotter(off_screen=True, window_size=size)
        p.set_background("white")
        for m, c in items:
            p.add_mesh(_pv(m), color=c, smooth_shading=False, specular=0.25)
        p.camera_position = cam
        p.camera.up = up
        p.reset_camera()
        p.camera.zoom(1.15)
        path = os.path.join(out_dir, name)
        p.screenshot(path)
        p.close()
        shots.append(path)

    cy = 135
    assembled = [(front, orange), (back, black)] + [(d[k], cols[k]) for k in ("glass", "sclera", "iris", "pupil", "joy")]
    shot("render_front.png", assembled, [(160, cy + 120, -420), (0, cy, 20), (0, 1, 0)])
    shot("render_back.png", assembled, [(-170, cy + 120, 460), (0, cy, 20), (0, 1, 0)])
    # exploded: front half open, back half laid beside it face-up
    back_open = back.rotate((0, 180, 0)).translate((150, 0, E.DEPTH))
    joy_open = d["joy"].rotate((0, 180, 0)).translate((150, 0, E.DEPTH))
    inside = [(front, orange), (back_open, black), (joy_open, cols["joy"])] + \
             [(d[k], cols[k]) for k in ("glass", "feather", "batt")]
    shot("render_inside.png", inside, [(75, cy - 60, 420), (75, cy, 0), (0, 1, 0)], size=(1400, 1200))
    for s_ in shots:
        print("wrote", s_)
    return shots


def sections(m, zs, out_path, extras=()):
    """Plot slices of manifold m at depths zs (profile frame)."""
    fig, axes = plt.subplots(1, len(zs), figsize=(4.2 * len(zs), 9), dpi=100)
    for ax, z in zip(np.atleast_1d(axes), zs):
        from matplotlib.path import Path
        from matplotlib.patches import PathPatch
        verts, codes = [], []
        for poly in m.slice(z).to_polygons():
            p = np.vstack([poly, poly[:1]])
            verts.append(p)
            codes += [Path.MOVETO] + [Path.LINETO] * (len(p) - 2) + [Path.CLOSEPOLY]
            ax.plot(p[:, 0], p[:, 1], "k-", lw=0.6)
        if verts:
            ax.add_patch(PathPatch(Path(np.vstack(verts), codes), fc="0.6", ec="none", alpha=0.7))
        for label, rects in extras:
            for (x0, x1, y0, y1, z0, z1) in rects:
                if z0 <= z <= z1:
                    ax.add_patch(plt.Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, ec="r", lw=0.8))
        ax.set_aspect("equal")
        ax.set_title(f"z = {z}")
        ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)
    print("wrote", out_path)
