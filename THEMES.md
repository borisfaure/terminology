# Themes in Terminology

This document describes how Terminology interacts with themes and what needs
to be handled by themes.

See `ChangeLog.theme` on changes related to themes.

# Color classes

Based on the chosen color scheme, Terminology sets the following color classes
on all the following edje groups:

* `BG`: the background color of the terminal
* `FG`: the default foreground color
* `CURSOR`: the color of the cursor
* `GLOW`: the color in the UI to ouline elements
* `HIGHLIGHT`: the color used as main color when an element is highlighted
* `GLOW_TXT`: text colors with some glow
* `GLOW_TXT_HIGHLIGHT`: text with glow that is highlighted
* `TAB_MISSED`: the number of tabs where a bell has rung, tabs that need
  attention
* `TAB_MISSED_OVER`: same but when the mouse is over that number
* `TAB_TITLE`: the colors of the active tab title
* `BG_SENDFILE` is the background color when there is a sendfile action. See
  `man tysend`. It is set to `#404040`.
* `END_SELECTION`: on selections, the color of the handles used to expand or shrink
  the area of the selection
* `/fg/normal/term/selection/arrow/left`,
  `/fg/normal/term/selection/arrow/down`,
  `/fg/normal/term/selection/arrow/up`,
  `/fg/normal/term/selection/arrow/right` replace `END_SELECTION` and are used
  to control the different arrows used to change the area of the selection

The following table explains how color classes are set from color scheme
values:

| Color Class          | Object color     | Outline color    | Shadow color  |
| -------------------- | ---------------- | ---------------- | ------------- |
| `BG`                 | `Colors.bg`      | `Colors.bg`      | `Colors.bg`   |
| `FG`                 | `Normal.def`     | `Normal.def`     | `Normal.def`  |
| `CURSOR`             | `Colors.main`    | `Colors.main`    | `Colors.main` |
| `GLOW`               | `Colors.main`    | `Colors.main`    | `Colors.main` |
| `GLOW_TXT_HIGHLIGHT` | `Colors.hl`      | `Colors.main`    | `Colors.main` |
| `END_SELECTION`      | `Colors.end_sel` | `Colors.end_sel` | `Colors.end_sel` |
| `TAB_MISSED`         | `Colors.tab_missed_1` | `Colors.tab_missed_2` | `Colors.tab_missed_3` |
| `TAB_MISSED_OVER`    | `Colors.tab_missed_1` | `Colors.tab_missed_2` | `Colors.tab_missed_3` |
| `TAB_TITLE`          | `Normal.def` | `Colors.tab_title_2` | `Colors.bg`   |
| `BG_SENDFILE`        | `#404040`    | `#404040`            | `#404040`     |



Let's dive into the edje groups that Terminology uses.

# Group `terminology/base`
All windows contain a group `terminology/base`.
Contains what is global to a window.

## Swallowed parts

### Part `terminology.content`
where `terminology/background`, or a split widget can be swallowed.

### Part `terminology.cmdbox`
To swallow a command box.
It reacts to the following signals:
* `cmdbox,show`
* `cmdbox,hide`

### Part `terminology.about`
It reacts to the following signals:
* `about,show`
* `about,hide`

### Part `terminology.optdetails`
Detailed settings panel.
It reacts to the following signals:
* `optdetails,show`
* `optdetails,hide`
It emits the following signal:
* `optdetails,hide,done`

### Part `terminology.options`
It reacts to the following signals:
* `options,show`
* `options,hide`

### Part `terminology.controls`
The controls box shown on right click.
It reacts to the following signals:
* `controls,show`
* `controls,hide`

## Special parts

### Part `youtube.txt`
A text part used to hold `YouTube channel` when the About is shown.

### Part `twitter.txt`
A text part used to hold `YouTube channel` when the About is shown.

## Signals emitted

### Signal `about,twitter`
When the Twitter link on the About page is clicked.

### Signal `about,twitter,ctx`
When the Twitter link on the About page is right-clicked.

### Signal `about,youtube`
When the YouTube link on the About page is clicked.

### Signal `about,youtube,ctx`
When the YouTube link on the About page is right-clicked.

### Signal `optdetails,hide,done`
When the swallowed part `terminology.optdetails` is finally hidden.


## Signals received

### Signals `about,show` and `about,hide`
Used to hide or show the about page, swallowed in `terminology.about`.

### Signals `controls,show` and `controls,hide`
Used to hide or show the controls, swallowed in `terminology.controls`.

### Signals `options,show` and `options,hide`
Used to hide or show the options panel, swallowed in `terminology.options`.

### Signals `optdetails,show` and `optdetails,hide`
Used to hide or show the options details settings panel, swallowed in `terminology.optdetails`.

### Signals `cmdbox,show` and `cmdbox,hide`
Used to hide or show the command box, swallowed in `terminology.cmdbox`.



# Group `terminology/background`

## Swallowed parts

### Parts `terminology.content`
Here is swallowed an object of group `terminology.background`.

## Special parts

### Parts `tabdrag` and `tabmiddle`
Their geometry are used to adjust drag values.

### Parts `drag_left_outline`, `drag_right_outline`, `drag_top_outline`, `drag_bottom_outline` and `terminology.tabregion`
Their geometry are used to know when the cursor enters them when dragging a
tab.

## Signals received
### Signals `tabbar,off` and `tabbar,on`
Whether to display a tab bar. Default is off.
### Signals `tab_btn,off` and `tab_btn,on`
Whether to display a tab button to easily navigate through tabs. Default is off.
### Signals `drag_left,on`, `drag_right,on`, `drag_top,on`, `drag_bottom,on`
When to start an animation when the cursor enters `drag_XXXX_outline` while
dragging a tab.
### Signals `drag_left,off`, `drag_right,off`, `drag_top,off`, `drag_bottom,off`
When to stop an animation started by the related `*,on` signals.
### Signals `drag_over_tabs,on` and `drag_over_tabs,off`
When the mouse, while dragging a tab, enters or leaves the tab region.
### Signals `grouped,on` and `grouped,off`
When input is broadcast to multiple terminals.

## Signal emitted
### Signal `tab,hdrag`
To notify that the current tab is being dragged.
### Signal `tab,drag,stop`
To notify that the current tab is no longer being dragged.
### Signal `tab,drag,move`
To notify that the current tab is being dragged outside of other tabs.




# Group `terminology/core`

## Swallowed parts

### Part `terminology.fade`

Part used to fade the background, either with a solid color or the screen
background when the `translucent` option is set.

### Part `terminology.background`

Actual background.
It reacts to the following signals based on the media to play in background:
* `media,off`
* `media,image`
* `media,scale`
* `media,edje`
* `media,movie`

### Part `terminology.tabregion`
Here is swalloed a fully transparent rectangle to move down the textgrid.

### Part `terminology.content`
Where actual text grid goes.

## Signals received
### Signals `tabbar,off` and `tabbar,on`
Whether to display a tab bar. Default is off.
### Signals `tab_btn,off` and `tab_btn,on`
Whether to display a tab button to easily navigate through tabs. Default is off.
### Signals `hdrag,on` and `hdrag,off`
Whether to restrict (default) horizontal tab drag

## Signal emitted
### Signals `tab,drag` and `tab,drag,stop`
To notify that the current tab is being dragged.
### Signals `tab,mouse,down`
Whenever the left mouse button is pressed on a tab.



# Group `terminology/about`
## Text parts
### Part `terminology.text`
The text of the __About__ message.



# Group `terminology.tabbar_back`
An inactive tab item
## Text parts
### Part `terminology.title`
Title of the tab.

## Signal emitted
### Signal `tab,activate`
When clicked on it, to notify that the user wants to go to that tab.

## Signals received
### Signal `bell`
To mark the tab as having missed a bell.
### Signal `bell,off`
To unmark the tab as having missed a bell.
### Signals `grouped,on` and `grouped,off`
When input is broadcast to multiple terminals.



# Group `terminology/keybinding`
__TODO__



# Group `terminology/miniview`
__TODO__



# Group `terminology/fontpreview`
__TODO__



# Group `terminology/selection`
An object used when selecting text.

## Parts

### Parts `terminology.top_left` and `terminology.bottom_right`
Swallow parts used to communicate via min/max size the size of the top and
bottom lines.


# Group `terminology/cursor`
__TODO__



# Group `terminology/cursor_bar`
__TODO__



# Group `terminology/cursor_underline`
__TODO__



# Group `terminology/sel/base`
__TODO__



# Group `terminology/sel/item`
__TODO__



# Group `terminology/link`
An object overlayd on text that is a link.



# Group `terminology/mediabusy`
__TODO__



# Group `terminology/mediactrl`
__TODO__



# Group `terminology/tab_drag_thumb`
A thumbnail of a tab being dragged.

## Swallowed part
### Part `terminology.content`
Here is swallowed an object of group `terminology.background`.

## Text part
### Part `terminology.title`
Title of the tab.

## Signal received
### Signal `bell`
To mark the tab as having missed a bell.



# Group `terminology/colorscheme_preview`
A group to preview a colorscheme. Used in the Colors panel in the settings.

## Swallowed part
### Part `terminology.content`
Where a textgrid showing off a color scheme is displayed.



# Group `terminology/color_preview`
A group to preview a color in a tooltip. The color is defined by the color
class `color_preview`.

## Text part
### Part `name`
Name of the color being previewed
