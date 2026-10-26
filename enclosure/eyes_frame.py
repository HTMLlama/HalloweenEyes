#!/usr/bin/env python3
"""
eyes_frame.py -- printable "slingshot" frame for HaloweenEyesV2.

A Y-shaped, two-piece clamshell:
  * two 1.5" Waveshare RGB OLED modules in the fork tips, facing the audience
  * KY-023 thumb joystick in the fork, facing you
  * Feather M4 Express (flat, dupont jumpers still plugged in) at the butt,
    USB port out the bottom of the handle
  * MakerFocus 3000 mAh LiPo (65 x 36 x 10) flat in the handle
  * mini toggle switch (M6 bushing) low on the side of the handle

Everything is parametric -- edit the numbers below and re-run:

    python3 eyes_frame.py            # writes stl/*.stl
    python3 eyes_frame.py --preview  # also renders render_*.png (needs pyvista)

Needs:  pip install manifold3d numpy   (+ pyvista matplotlib for --preview)

Coordinates (the "profile frame" used throughout):
    x = left/right, y = up (butt of the handle at y = 0),
    z = depth: z = 0 is the FRONT face (audience side), z = DEPTH is the BACK
    face (your side). NOTE: seen from the audience, +x is on THEIR LEFT.
"""
import os
import sys

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold, OpType

# ----------------------------------------------------------------------------
# Parameters
# ----------------------------------------------------------------------------
SEG = 96  # circle smoothness

# Shell
WALL = 2.4          # wall thickness
DEPTH = 41.0        # total front-to-back thickness (set by the Feather's duponts)
SPLIT = 16.0        # z of the seam between the front and back halves
CHAMFER_FRONT = 2.0
CHAMFER_BACK = 2.0
LIP_H = 2.0         # alignment lip on the back half that drops into the front half
LIP_W = 1.2
LIP_CLEAR = 0.25

# Overall layout
EYE_SPACING = 72.0  # centre-to-centre distance of the two screens
EYE_Y = 238.0       # height of the screen centres above the butt
HANDLE_W = 42.8     # outside width of the handle
JOY_Y = 176.0       # height of the joystick's stick above the butt
HUB_R = 27.0        # radius of the round hub around the joystick
ARM_R = 12.0        # half-width of the fork arms

# Waveshare 1.5" RGB OLED module (mounted rotated: 37 wide x 44.5 tall,
# header holes at the top, cable exiting the bottom -- as in the photo)
OLED_W, OLED_H = 37.0, 44.5
OLED_CLEAR = 0.6                # pocket clearance around the PCB
OLED_HOLE_DX, OLED_HOLE_DY = 16.0, 19.75   # +-offsets of the 4 M2 holes
OLED_GLASS_T = 2.8              # PCB front face -> front of glass (boss height)
OLED_WINDOW = 27.4              # square window, inside face (active area is 26.86 square)
OLED_WINDOW_R = 0.8             # window corner radius
OLED_WINDOW_DX = -1.4           # active area sits 1.4 mm toward the audience's
                                # right (away from the glass flex tail)
POD_W = OLED_W + 2 * OLED_CLEAR + 2 * WALL
POD_H = OLED_H + 2 * OLED_CLEAR + 2 * WALL
POD_TOP_EXTRA = 8.0             # room above the module for a case screw
POD_R = 5.0

# Feather M4 Express (lies flat, parts + duponts facing your side)
FEATHER_L, FEATHER_W = 50.8, 22.86
FEATHER_Y0 = WALL + 1.2         # USB end of the board
FEATHER_X_RIGHT = 16.8          # board edge nearest the +x wall (duponts clear the lip)
FEATHER_STANDOFF = 4.0          # room under the board for header pin tails
FEATHER_HOLES = [(2.54, 2.54), (2.54, 20.32), (48.26, 2.54), (48.26, 20.32)]
USB_OPEN_W, USB_OPEN_H = 13.0, 8.0   # plug overmold opening in the butt

# Battery
BATT_L, BATT_W, BATT_T = 66.0, 37.0, 11.0   # 65 x 36 x 10 plus clearance
BATT_Y0 = 76.0

# Toggle switch (on the -x side = your LEFT when you hold it)
SWITCH_Y = 65.0
SWITCH_Z = 9.0
SWITCH_HOLE_D = 6.4

# KY-023 joystick (header end pointing down the handle)
JOY_HOLE_DX, JOY_HOLE_DY = 20.5, 26.5  # mounting-hole spacing (x, y)
JOY_PCB_W, JOY_PCB_L = 26.0, 34.0
JOY_PCB_ABOVE = 17.0            # PCB extends this far above the stick centre
JOY_HEIGHT = 13.0               # PCB top face -> inside of the back wall
JOY_OPEN_D = 30.0               # opening for the domed boot
JOY_OPEN_CHAMFER = 2.0

# Heat-set inserts (hole diameter / depth) -- match these to your kit
INS_M2 = (3.0, 4.0)
INS_M25 = (3.5, 5.0)
INS_M3 = (4.0, 6.0)
M3_CLEAR = 3.4
M3_HEAD_D = 6.4
CASE_SCREW_LEN = 20.0           # M3 x 20 socket-head case screws
CASE_PILLAR_R_FRONT = 4.0
CASE_PILLAR_R_BACK = 4.4

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stl")

# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------
ROUND = JoinType.Round


def circle(r, x=0.0, y=0.0):
    return CrossSection.circle(r, SEG).translate((x, y))


def rrect(w, h, r, cx=0.0, cy=0.0):
    core = CrossSection.square((w - 2 * r, h - 2 * r), True)
    return core.offset(r, ROUND, 2.0, SEG).translate((cx, cy))


def rect(w, h, cx=0.0, cy=0.0):
    return CrossSection.square((w, h), True).translate((cx, cy))


def union(items):
    items = [i for i in items if i is not None]
    if isinstance(items[0], CrossSection):
        return CrossSection.batch_boolean(items, OpType.Add)
    return Manifold.batch_boolean(items, OpType.Add)


def extrude(cs, z0, z1):
    return Manifold.extrude(cs, z1 - z0).translate((0, 0, z0))


def cyl(r, z0, z1, x=0.0, y=0.0, r_top=None):
    r_top = r if r_top is None else r_top
    return Manifold.cylinder(z1 - z0, r, r_top, SEG).translate((x, y, z0))


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def hcyl_x(r, x0, x1, y, z):
    """Cylinder along the x axis."""
    return Manifold.cylinder(x1 - x0, r, r, SEG).rotate((0, 90, 0)).translate((x0, y, z))


# ----------------------------------------------------------------------------
# Derived positions
# ----------------------------------------------------------------------------
POD_XS = (-EYE_SPACING / 2, EYE_SPACING / 2)
POD_CY = EYE_Y + POD_TOP_EXTRA / 2           # pod outline centre
POD_TOTAL_H = POD_H + POD_TOP_EXTRA
POD_BOTTOM = EYE_Y - POD_H / 2

FEATHER_Z0 = WALL + FEATHER_STANDOFF         # back face of the Feather PCB


def feather_xy(u, v):
    """Board coords (u from USB end, v from the +x edge) -> profile x, y."""
    return FEATHER_X_RIGHT - v, FEATHER_Y0 + u


JOY_PCB_Z = DEPTH - WALL - JOY_HEIGHT        # top face of the joystick PCB

# Case screws: (x, y). Front half gets an M3 insert, back half the screw.
CASE_SCREWS = [
    (POD_XS[0], EYE_Y + OLED_H / 2 + OLED_CLEAR + CASE_PILLAR_R_BACK + 0.2),
    (POD_XS[1], EYE_Y + OLED_H / 2 + OLED_CLEAR + CASE_PILLAR_R_BACK + 0.2),
    (0.0, JOY_Y + 22.0),                     # top of the hub, above the stick
    (-15.0, 149.0),                          # top of the handle
    (15.0, 149.0),
    (15.0, SWITCH_Y),                        # between Feather and battery
    (-15.0, 6.4),                            # butt, beside the USB end
]


# ----------------------------------------------------------------------------
# Outline
# ----------------------------------------------------------------------------
def outline():
    handle = rrect(HANDLE_W, JOY_Y + 4, 5.0, 0, (JOY_Y + 4) / 2)
    hub = circle(HUB_R, 0, JOY_Y)
    parts = [handle, hub]
    for s in (-1, 1):
        px = POD_XS[0 if s < 0 else 1]
        arm = CrossSection.batch_hull([
            circle(ARM_R, s * 7.0, JOY_Y + 10.0),
            circle(ARM_R, px, POD_BOTTOM + ARM_R - 2.0),
        ])
        pod = rrect(POD_W, POD_TOTAL_H, POD_R, px, POD_CY)
        parts += [arm, pod]
    prof = union(parts)
    # fillet the inside corners (crotch, arm/pod joints)
    return prof.offset(6.0, ROUND, 2.0, SEG).offset(-6.0, ROUND, 2.0, SEG)


PROFILE = outline()
INNER = PROFILE.offset(-WALL, ROUND, 2.0, SEG)


def chamfered_solid(prof):
    """Extrude with 45-degree chamfers on the front and back edges, built as
    0.2 mm slices (one print layer each, so the steps never show)."""
    step = 0.2
    slices = []
    z = 0.0
    while z < CHAMFER_FRONT - 1e-6:
        inset = CHAMFER_FRONT - z - step
        slices.append(extrude(prof.offset(-inset, ROUND, 2.0, SEG) if inset > 0 else prof, z, z + step + 0.05))
        z += step
    slices.append(extrude(prof, CHAMFER_FRONT - 0.05, DEPTH - CHAMFER_BACK + 0.05))
    z = DEPTH - CHAMFER_BACK
    while z < DEPTH - 1e-6:
        inset = z + step - (DEPTH - CHAMFER_BACK)
        slices.append(extrude(prof.offset(-inset, ROUND, 2.0, SEG), z - 0.05 if z > DEPTH - CHAMFER_BACK else z, z + step))
        z += step
    return union(slices)


# ----------------------------------------------------------------------------
# Body
# ----------------------------------------------------------------------------
def build():
    outer = chamfered_solid(PROFILE)
    solid_envelope = extrude(PROFILE, -1, DEPTH + 1)

    cavity = [extrude(INNER, WALL, DEPTH - WALL)]
    for px in POD_XS:
        pocket = rrect(OLED_W + 2 * OLED_CLEAR, OLED_H + 2 * OLED_CLEAR, 2.0, px, EYE_Y)
        cavity.append(extrude(pocket, WALL, DEPTH - WALL))
    body = outer - union(cavity)

    front_add, front_cut, back_add, back_cut = [], [], [], []

    # --- OLED screens ------------------------------------------------------
    glass_keepout = []
    for px in POD_XS:
        wx = px + OLED_WINDOW_DX
        # 45-degree bevel: widest at the front face, exactly OLED_WINDOW at the glass
        w_out = OLED_WINDOW + 2 * WALL
        front_cut.append(Manifold.batch_hull([
            extrude(rrect(w_out, w_out, OLED_WINDOW_R + WALL, wx, EYE_Y), -0.01, 0.0),
            extrude(rrect(OLED_WINDOW, OLED_WINDOW, OLED_WINDOW_R, wx, EYE_Y), WALL, WALL + 0.01),
        ]))
        # glass + flex tail footprint (bosses must not touch it)
        glass_keepout.append(extrude(rect(37.0, 35.2, px, EYE_Y), WALL - 0.1, WALL + OLED_GLASS_T))
        for dx in (-OLED_HOLE_DX, OLED_HOLE_DX):
            for dy in (-OLED_HOLE_DY, OLED_HOLE_DY):
                x, y = px + dx, EYE_Y + dy
                front_add.append(cyl(2.3, WALL - 0.5, WALL + OLED_GLASS_T, x, y))
                front_cut.append(cyl(INS_M2[0] / 2, WALL + OLED_GLASS_T - INS_M2[1], WALL + OLED_GLASS_T + 0.1, x, y))
    glass_keepout = union(glass_keepout)

    # --- Feather -------------------------------------------------------------
    for u, v in FEATHER_HOLES:
        x, y = feather_xy(u, v)
        front_add.append(cyl(2.9, WALL - 0.5, FEATHER_Z0, x, y))
        front_cut.append(cyl(INS_M25[0] / 2, FEATHER_Z0 - INS_M25[1], FEATHER_Z0 + 0.1, x, y))
    # USB opening in the butt, centred on the connector
    usb_x, _ = feather_xy(0, FEATHER_W / 2)
    usb_z = FEATHER_Z0 + 1.6 + 1.6
    front_cut.append(box(usb_x - USB_OPEN_W / 2, usb_x + USB_OPEN_W / 2, -1, WALL + 0.5,
                         usb_z - USB_OPEN_H / 2, usb_z + USB_OPEN_H / 2))

    # --- Battery cradle (end stops; the side walls locate it in x) ----------
    stop_h = 6.0
    front_add.append(box(-BATT_W / 2, BATT_W / 2, BATT_Y0 + BATT_L, BATT_Y0 + BATT_L + 2.0, WALL - 0.5, WALL + stop_h))
    for sx in (-1, 1):  # bottom stop, gap in the middle for the leads
        front_add.append(box(sx * BATT_W / 2 - (sx > 0) * 10.0, sx * BATT_W / 2 + (sx < 0) * 10.0,
                             BATT_Y0 - 2.0, BATT_Y0, WALL - 0.5, WALL + stop_h))
    # hold-down posts from the back half
    for bx in (-12.0, 12.0):
        for by in (BATT_Y0 + 10.0, BATT_Y0 + BATT_L - 10.0):
            back_add.append(cyl(2.5, WALL + BATT_T + 0.8, DEPTH - WALL + 0.5, bx, by))

    # --- Switch --------------------------------------------------------------
    front_cut.append(hcyl_x(SWITCH_HOLE_D / 2, -HANDLE_W / 2 - 1, -HANDLE_W / 2 + WALL + 1, SWITCH_Y, SWITCH_Z))

    # --- Joystick ------------------------------------------------------------
    jz0 = DEPTH - WALL
    r_open = JOY_OPEN_D / 2
    back_cut.append(cyl(r_open, jz0 - 0.1, DEPTH - JOY_OPEN_CHAMFER, 0, JOY_Y))
    back_cut.append(cyl(r_open, DEPTH - JOY_OPEN_CHAMFER, DEPTH + 0.01, 0, JOY_Y, r_top=r_open + JOY_OPEN_CHAMFER + 0.01))
    for dx in (-JOY_HOLE_DX / 2, JOY_HOLE_DX / 2):
        for dy in (-JOY_HOLE_DY / 2, JOY_HOLE_DY / 2):
            back_add.append(cyl(3.0, JOY_PCB_Z, jz0 + 0.5, dx, JOY_Y + dy))
            back_cut.append(cyl(INS_M25[0] / 2, JOY_PCB_Z - 0.1, JOY_PCB_Z + INS_M25[1], dx, JOY_Y + dy))

    # --- Case screws -----------------------------------------------------------
    for x, y in CASE_SCREWS:
        front_add.append(cyl(CASE_PILLAR_R_FRONT, WALL - 0.5, SPLIT, x, y))
        front_cut.append(cyl(INS_M3[0] / 2, SPLIT - INS_M3[1], SPLIT + 0.1, x, y))
        back_add.append(cyl(CASE_PILLAR_R_BACK, SPLIT, DEPTH - WALL + 0.5, x, y))
        head_z = SPLIT + CASE_SCREW_LEN - INS_M3[1] + 1.0   # underside of the screw head
        back_cut.append(cyl(M3_CLEAR / 2, SPLIT - 0.1, DEPTH + 0.1, x, y))
        back_cut.append(cyl(M3_HEAD_D / 2, head_z, DEPTH + 0.1, x, y))

    # --- Alignment lip -------------------------------------------------------
    lip_cs = INNER.offset(-LIP_CLEAR, ROUND, 2.0, SEG) - INNER.offset(-LIP_CLEAR - LIP_W, ROUND, 2.0, SEG)
    lip_keep_clear = union([circle(CASE_PILLAR_R_FRONT + 0.5, x, y) for x, y in CASE_SCREWS])
    lip_cs = lip_cs - lip_keep_clear
    lip = extrude(lip_cs, SPLIT - LIP_H, SPLIT + 0.01)
    # tie the lip to the back half's wall (the lip itself stands off by LIP_CLEAR)
    lip_root = INNER.offset(0.5, ROUND, 2.0, SEG) - INNER.offset(-LIP_CLEAR - LIP_W, ROUND, 2.0, SEG)
    lip = lip + extrude(lip_root - lip_keep_clear, SPLIT, SPLIT + 1.5)

    # --- Assemble halves -------------------------------------------------------
    front_add_m = (union(front_add) ^ solid_envelope) - glass_keepout
    back_add_m = union(back_add) ^ solid_envelope

    front = (body ^ box(-500, 500, -500, 500, -1, SPLIT)) + front_add_m
    front = (front - union(front_cut)) ^ outer    # nothing pokes out of the skin

    back = (body ^ box(-500, 500, -500, 500, SPLIT, DEPTH + 1)) + back_add_m + lip
    back = (back - union(back_cut)) ^ outer
    return front, back


# ----------------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------------
def to_print_front(m):
    """Front face down on the bed -- it already is (z = 0)."""
    return m


def to_print_back(m):
    """Back face down on the bed: rotate 180 degrees about the y axis."""
    return m.rotate((0, 180, 0)).translate((0, 0, DEPTH))


def write_stl(m, path):
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3].astype(np.float32)
    t = np.asarray(mesh.tri_verts, dtype=np.int64)
    tri = v[t]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    n = (n / np.where(ln == 0, 1, ln)).astype(np.float32)
    rec = np.zeros(len(t), dtype=[("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
    rec["n"], rec["v"] = n, tri
    with open(path, "wb") as f:
        f.write(b"HaloweenEyesV2 frame".ljust(80, b" "))
        f.write(np.uint32(len(t)).tobytes())
        f.write(rec.tobytes())


def fit_tests(front, back):
    """Small coupons to check the OLED and joystick fit before the big print."""
    px = POD_XS[1]
    oled = front ^ box(px - POD_W / 2 - 1, px + POD_W / 2 + 1, POD_BOTTOM - 1, EYE_Y + POD_TOTAL_H,
                       -1, WALL + OLED_GLASS_T + 0.5)
    joy = back ^ box(-HUB_R - 1, HUB_R + 1, JOY_Y - 24, JOY_Y + HUB_R + 1, JOY_PCB_Z - 0.5, DEPTH + 1)
    return oled, joy


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    front, back = build()
    oled_fit, joy_fit = fit_tests(front, back)
    outputs = {
        "eyes_frame_front.stl": to_print_front(front),
        "eyes_frame_back.stl": to_print_back(back),
        "fit_test_oled.stl": to_print_front(oled_fit),
        "fit_test_joystick.stl": to_print_back(joy_fit),
    }
    for name, m in outputs.items():
        path = os.path.join(OUT_DIR, name)
        write_stl(m, path)
        bb = m.bounding_box()
        print(f"{name:28s} {bb[3]-bb[0]:6.1f} x {bb[4]-bb[1]:6.1f} x {bb[5]-bb[2]:5.1f} mm"
              f"  vol {m.volume()/1000:6.1f} cm3  genus {m.genus()}")
    if "--preview" in sys.argv:
        import preview
        preview.render(front, back, os.path.dirname(OUT_DIR), sys.modules[__name__])


if __name__ == "__main__":
    main()
