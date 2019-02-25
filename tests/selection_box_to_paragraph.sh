#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

#move to 2,0
printf '\033[2H'

# set text
TEXT1='Cras nec porttitor urna. Aliquam vel tellus ligula. In lacinia lorem neque, ac blandit purus sagittis sed. Nullam rhoncus feugiat felis, vel lobortis nunc varius eget. Cras tempor diam ut commodo imperdiet. Donec id odio dolor. Ut cursus mauris sed suscipit aliquam. Interdum et malesuada fames ac ante ipsum primis in faucibus. Donec ut diam ullamcorper, pellentesque libero nec, lobortis turpis. Praesent efficitur ipsum ut turpis elementum, in vestibulum augue suscipit. Donec dignissim justo id rhoncus sodales.'

TEXT2='Mauris vulputate magna massa. Donec in magna mi. Aliquam feugiat vestibulum orci sed bibendum. Aenean ornare placerat arcu, eu blandit lacus feugiat sit amet. Phasellus lobortis cursus rutrum. Praesent pulvinar velit non maximus efficitur. Mauris non velit eget enim tempus sodales. Donec sodales lorem quis diam iaculis, vel finibus diam aliquam. In interdum faucibus eleifend. Proin pellentesque, nisi a congue euismod, nibh ipsum luctus diam, et iaculis elit arcu vel tellus.'

TEXT3='Proin tristique sagittis ornare. Phasellus egestas aliquet euismod. Duis maximus lacinia erat, in sollicitudin sem interdum in. Fusce vel vulputate justo. Fusce lacinia sapien quis quam malesuada elementum. Phasellus consectetur bibendum mi, non tempus sem porttitor non. Sed neque velit, fringilla eget mollis in, pharetra quis augue. Nunc et tristique augue. Quisque volutpat arcu ut dictum euismod. Suspendisse quis rhoncus mauris, in congue ligula. Proin tristique nisl vel tempus volutpat.'

# display text
printf "%s\r\n%s\r\n%s" "$TEXT1" "$TEXT2" "$TEXT3"
# force render
printf '\033}tr\0'

remove_selection()
{
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
}


## sel "top-down" to down
# mouse down to start selection
printf '\033}td;395;148;1;4;0\0'
# mouse move
printf '\033}tm;25;160;4\0'
# mouse up
printf '\033}tu;25;160;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsd bibendum. Aenean ornare placerat arcu, eu blandit la\nellus lobortis cursus rutrum. Praesent pulvinar velit\0'
# To
# mouse move
printf '\033}tm;210;248;0\0'
# mouse triple-click with shift
printf '\033}td;210;248;1;2;2\0'
printf '\033}tu;210;248;1;2;2\0'
# selection is
printf '\033}ts sed bibendum. Aenean ornare placerat arcu, eu blandit lacus feugiat sit amet. P\nhasellus lobortis cursus rutrum. Praesent pulvinar velit non maximus efficitur.\nMauris non velit eget enim tempus sodales. Donec sodales lorem quis diam iaculis\n, vel finibus diam aliquam. In interdum faucibus eleifend. Proin pellentesque, n\nisi a congue euismod, nibh ipsum luctus diam, et iaculis elit arcu vel tellus.\n\nProin tristique sagittis ornare. Phasellus egestas aliquet euismod. Duis maximus\n lacinia erat, in sollicitudin sem interdum in. Fusce vel vulputate justo. Fusce\n lacinia sapien quis quam malesuada elementum. Phasellus consectetur bibendum mi\n, non tempus sem porttitor non. Sed neque velit, fringilla eget mollis in, phare\ntra quis augue. Nunc et tristique augue. Quisque volutpat arcu ut dictum euismod\n. Suspendisse quis rhoncus mauris, in congue ligula. Proin tristique nisl vel te\nmpus volutpat.\n\0'
remove_selection



## sel "top-down" to up
# mouse down to start selection
printf '\033}td;395;148;1;4;0\0'
# mouse move
printf '\033}tm;25;160;4\0'
# mouse up
printf '\033}tu;25;160;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsd bibendum. Aenean ornare placerat arcu, eu blandit la\nellus lobortis cursus rutrum. Praesent pulvinar velit\0'
# To
# mouse move
printf '\033}tm;500;78;0\0'
# mouse triple-click with shift
printf '\033}td;500;78;1;2;2\0'
printf '\033}tu;500;78;1;2;2\0'
# selection is
printf '\033}tsCras nec porttitor urna. Aliquam vel tellus ligula. In lacinia lorem neque, ac b\nlandit purus sagittis sed. Nullam rhoncus feugiat felis, vel lobortis nunc variu\ns eget. Cras tempor diam ut commodo imperdiet. Donec id odio dolor. Ut cursus ma\nuris sed suscipit aliquam. Interdum et malesuada fames ac ante ipsum primis in f\naucibus. Donec ut diam ullamcorper, pellentesque libero nec, lobortis turpis. Pr\naesent efficitur ipsum ut turpis elementum, in vestibulum augue suscipit. Donec\ndignissim justo id rhoncus sodales.\n\nMauris vulputate magna massa. Donec in magna mi. Aliquam feugiat vestibulum orci\n sed bibendum. Aenean ornare placerat arcu, eu blandit lacus feugiat sit amet. P\nhasellus lobortis cursus rutrum. Praesent pulvinar velit non maximus efficitur.\0'
remove_selection


## sel "top_down" to within selection
# mouse down to start selection
printf '\033}td;395;148;1;4;0\0'
# mouse move
printf '\033}tm;25;160;4\0'
# mouse up
printf '\033}tu;25;160;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsd bibendum. Aenean ornare placerat arcu, eu blandit la\nellus lobortis cursus rutrum. Praesent pulvinar velit\0'
# To
# mouse move
printf '\033}tm;500;150;0\0'
# mouse triple-click with shift
printf '\033}td;500;150;1;2;2\0'
printf '\033}tu;500;150;1;2;2\0'
# selection is
printf '\033}tsMauris vulputate magna massa. Donec in magna mi. Aliquam feugiat vestibulum orci\n sed bibendum. Aenean ornare placerat arcu, eu blandit lacus feugiat sit amet. P\nhasellus lobortis cursus rutrum. Praesent pulvinar velit non maximus efficitur.\nMauris non velit eget enim tempus sodales. Donec sodales lorem quis diam iaculis\n, vel finibus diam aliquam. In interdum faucibus eleifend. Proin pellentesque, n\nisi a congue euismod, nibh ipsum luctus diam, et iaculis elit arcu vel tellus.\n\0'
remove_selection


## sel "down-top" to down
# mouse down to start selection
printf '\033}td;25;160;1;4;0\0'
# mouse move
printf '\033}tm;395;148;4\0'
# mouse up
printf '\033}tu;395;148;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsd bibendum. Aenean ornare placerat arcu, eu blandit la\nellus lobortis cursus rutrum. Praesent pulvinar velit\0'
# To
# mouse move
printf '\033}tm;210;248;0\0'
# mouse triple-click with shift
printf '\033}td;210;248;1;2;2\0'
printf '\033}tu;210;248;1;2;2\0'
# selection is
printf '\033}ts sed bibendum. Aenean ornare placerat arcu, eu blandit lacus feugiat sit amet. P\nhasellus lobortis cursus rutrum. Praesent pulvinar velit non maximus efficitur.\nMauris non velit eget enim tempus sodales. Donec sodales lorem quis diam iaculis\n, vel finibus diam aliquam. In interdum faucibus eleifend. Proin pellentesque, n\nisi a congue euismod, nibh ipsum luctus diam, et iaculis elit arcu vel tellus.\n\nProin tristique sagittis ornare. Phasellus egestas aliquet euismod. Duis maximus\n lacinia erat, in sollicitudin sem interdum in. Fusce vel vulputate justo. Fusce\n lacinia sapien quis quam malesuada elementum. Phasellus consectetur bibendum mi\n, non tempus sem porttitor non. Sed neque velit, fringilla eget mollis in, phare\ntra quis augue. Nunc et tristique augue. Quisque volutpat arcu ut dictum euismod\n. Suspendisse quis rhoncus mauris, in congue ligula. Proin tristique nisl vel te\nmpus volutpat.\n\0'
remove_selection


## sel "down-top" to up
# mouse down to start selection
printf '\033}td;25;160;1;4;0\0'
# mouse move
printf '\033}tm;395;148;4\0'
# mouse up
printf '\033}tu;395;148;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsd bibendum. Aenean ornare placerat arcu, eu blandit la\nellus lobortis cursus rutrum. Praesent pulvinar velit\0'
# To
# mouse move
printf '\033}tm;500;78;0\0'
# mouse triple-click with shift
printf '\033}td;500;78;1;2;2\0'
printf '\033}tu;500;78;1;2;2\0'
# selection is
printf '\033}tsCras nec porttitor urna. Aliquam vel tellus ligula. In lacinia lorem neque, ac b\nlandit purus sagittis sed. Nullam rhoncus feugiat felis, vel lobortis nunc variu\ns eget. Cras tempor diam ut commodo imperdiet. Donec id odio dolor. Ut cursus ma\nuris sed suscipit aliquam. Interdum et malesuada fames ac ante ipsum primis in f\naucibus. Donec ut diam ullamcorper, pellentesque libero nec, lobortis turpis. Pr\naesent efficitur ipsum ut turpis elementum, in vestibulum augue suscipit. Donec\ndignissim justo id rhoncus sodales.\n\nMauris vulputate magna massa. Donec in magna mi. Aliquam feugiat vestibulum orci\n sed bibendum. Aenean ornare placerat arcu, eu blandit lacus feugiat sit amet. P\nhasellus lobortis cursus rutrum. Praesent pulvinar velit non maximus efficitur.\0'
remove_selection


## sel "down-top" to within
# mouse down to start selection
printf '\033}td;25;160;1;4;0\0'
# mouse move
printf '\033}tm;395;148;4\0'
# mouse up
printf '\033}tu;395;148;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsd bibendum. Aenean ornare placerat arcu, eu blandit la\nellus lobortis cursus rutrum. Praesent pulvinar velit\0'
# To
# mouse move
printf '\033}tm;500;150;0\0'
# mouse triple-click with shift
printf '\033}td;500;150;1;2;2\0'
printf '\033}tu;500;150;1;2;2\0'
# selection is
printf '\033}tsMauris vulputate magna massa. Donec in magna mi. Aliquam feugiat vestibulum orci\n sed bibendum. Aenean ornare placerat arcu, eu blandit lacus feugiat sit amet. P\nhasellus lobortis cursus rutrum. Praesent pulvinar velit non maximus efficitur.\nMauris non velit eget enim tempus sodales. Donec sodales lorem quis diam iaculis\n, vel finibus diam aliquam. In interdum faucibus eleifend. Proin pellentesque, n\nisi a congue euismod, nibh ipsum luctus diam, et iaculis elit arcu vel tellus.\n\0'
remove_selection
