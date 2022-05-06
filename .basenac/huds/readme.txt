HUDs are a string of opcodes that describe how the HUD is laid out. Spaces are collapsed,
so feel free to use any spacing you want to use.

The cursor is where all HUD elements will be drawn. The following opcodes move the cursor:

xl <int>
 * Set the X position relative to the left side of the screen
xr <int>
 * Set the X position relative to the right side of the screen
yt <int>
 * Set the Y position relative to the top side of the screen
yb <int>
 * Set the Y position relative to the bottom side of the screen

For positioning relative to the center, the following opcodes can be used
which position the cursor to a virtual 320x240 screen centered on the crosshair
(such that 160,120 is the direct center):

xv <int>
yv <int>

Now that we've set our cursor where we want to do stuff, let's do stuff! You can
draw various things from here. The simplest draw opcodes are as follows:

picn <string>
 * Draw the specified image (relative to images/, extension not required;
   ie, `i_health` for `pics/i_health.pcx`)

hnum
 * Draw the current health value

anum
 * Draw the current ammo value

rnum
 * Draw the current armor value

cstring <string>
 * Draw a string, centered

cstring2 <string>
 * Draw a string using the alt font, centered

string <string>
 * Draw a string, left aligned

string2 <string>
 * Draw a string using the alt font, left aligned

color <color>
 * Change the active color. <color> is either a named color, or a hexadecimal string prefixed with #
   (similar to html; #rrggbb, #rgb, #rrggbbaa, or #rgba)

We've drawn static data, but we can also draw dynamic data derived from stats data given to the client!
The client has an array of stat data as integers, and we can pull them out to render dependent data.

For stat indices, you can use the following macros to refer to the stats the game stores:
 $STAT_HEALTH_ICON
 $STAT_HEALTH
 $STAT_AMMO_ICON
 $STAT_AMMO
 $STAT_ARMOR_ICON
 $STAT_ARMOR
 $STAT_SELECTED_ICON
 $STAT_PICKUP_ICON
 $STAT_PICKUP_STRING
 $STAT_TIMER_ICON
 $STAT_TIMER
 $STAT_HELPICON
 $STAT_SELECTED_ITEM
 $STAT_LAYOUTS
 $STAT_FRAGS
 $STAT_FLASHES
 $STAT_CHASE
 $STAT_SPECTATOR

pic <stat>
 * Draw the specified image at the specified stat index

num <num digits> <stat>
 * Draw the specified stat index with the specified digit count

stat_string <stat>
 * Draw the specified configstring referred to by the specified stat index

Now that we've drawn stuff, you probably want to be able to conditionally change what's
drawn based on some sort of state (ie, the state of a stat, or a configstring). We can
do that with control blocks.

Control blocks can be nested.
Positions are *not* saved/restored by control blocks, so be careful about changing positions
in them!

if <stat>
 * Begin a control block if the specified stat value is truthy

endif
 * End a control block