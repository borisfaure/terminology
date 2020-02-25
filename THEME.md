This document describes how Terminology interacts with themes and what needs
to be handled by themes.

See `ChangeLog.theme` on changes related to themes.


Windows contain group `terminology/base`.


# `terminology/base`
Contains what is global to a window.

## Swallowed parts

### `terminology.content`
where `terminology/background`, or a split widget can be swallowed.

### `terminology.cmdbox`
To swallow a command box.
It reacts to the following signals:
* `cmdbox,show`
* `cmdbox,hide`

### `terminology.about`
It reacts to the following signals:
* `about,show`
* `about,hide`

### `terminology.optdetails`
Detailed settings panel.
It reacts to the following signals:
* `optdetails,show`
* `optdetails,hide`
It emits the following signal:
* `optdetails,hide,done`

### `terminology.options`
It reacts to the following signals:
* `options,show`
* `options,hide`

### `terminology.controls`
The controls box shown on right click.
It reacts to the following signals:
* `controls,show`
* `controls,hide`



# `terminology/background`

## Swallowed parts

### `terminology.content`
Here is swallowed an object of group `terminology.background`.

### TODO

## Special parts

### `tabdrag` and `tabmiddle`
Their geometry are used to adjust drag values.

## Signal received
### `tabbar,off` and `tabbar,on`
Whether to display a tab bar. Default is off.
### `tab_btn,off` and `tab_btn,on`
Whether to display a tab button to easily navigate through tabs. Default is off.

### TODO

## Signal emitted
### `tab,hdrag`
To notify that the current tab is being dragged.
### `tab,drag,stop`
To notify that the current tab is no longer being dragged.
### `tab,drag,move`
To notify that the current tab is being dragged outside of other tabs.

### TODO



# `terminology/core`

## Swallowed parts

### `terminology.background`

Actual background.
It reacts to the following signals based on the media to play in background:
* `media,off`
* `media,image`
* `media,scale`
* `media,edje`
* `media,movie`

### `terminology.tabregion`
Here is swalloed a fully transparent rectangle to move down the textgrid.

### `terminology.content`
Where actual text grid goes.

## Signal received
### `tabbar,off` and `tabbar,on`
Whether to display a tab bar. Default is off.
### `tab_btn,off` and `tab_btn,on`
Whether to display a tab button to easily navigate through tabs. Default is off.
### `hdrag,on` and `hdrag,off`
Whether to restrict (default) horizontal tab drag

## Signal emitted
### `tab,drag` and `tab,drag,stop`
To notify that the current tab is being dragged.
### `tab,mouse,down`
Whenever the left mouse button is pressed on a tab.



# `terminology/about`
## Text parts
### `terminology.text`
The text of the __About__ message.



# `terminology.tabbar_back`
An inactive tab item
## Text parts
### `terminology.title`
Title of the tab.

## Signal emitted
### `tab,activate`
When clicked on it, to notify that the user wants to go to that tab.

## Signal received
### `bell`
To mark the tab as having missed a bell.
### `bell,off`
To unmark the tab as having missed a bell.
### `style,active`
To set the style as active, used during tab drag and drop.



# `terminology/keybinding`
__TODO__



# `terminology/miniview`
__TODO__



# `terminology/fontpreview`
__TODO__



# `terminology/selection`
An object used when selecting text.
__TODO__



# `terminology/cursor`
__TODO__



# `terminology/cursor_bar`
__TODO__



# `terminology/cursor_underline`
__TODO__



# `terminology/sel/base`
__TODO__



# `terminology/sel/item`
__TODO__



# `terminology/link`
An object overlayd on text that is a link.



# `terminology/mediabusy`
__TODO__



# `terminology/mediactrl`
__TODO__
