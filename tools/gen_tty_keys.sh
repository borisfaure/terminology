#!/bin/bash

if [ -z "$(type -p xdotool)" ]; then
	echo 'Install xdotool!' >&2
	exit 1
fi

if [ -z "$(type -p numlockx)" ]; then
	echo 'Install numlockx!' >&2
	exit 1
fi

if [ -t 1 ]; then
	echo 'Redirect the output to a file!' >&2
	exit 1
fi

function do_reset {
	echo -ne '\e[?1050l' >&2
	echo -ne '\e[?1051l' >&2
	echo -ne '\e[?1052l' >&2
	echo -ne '\e[?1053l' >&2
	echo -ne '\e[?1060l' >&2
	echo -ne '\e[?1061l' >&2
	echo -ne '\e[?1l' >&2  # normal cursor mode
	echo -ne '\e>' >&2     # normal keypad mode
}

do_key() {
   local key="$1"
   local shift="$2"
   local ctrl="$3"
   local alt="$4"
   local kind="$5"

   local mod=""
   local numlock=0

   if [ "$shift" = "1" ]; then
      mod="${mod}Shift+"
   fi
   if [ "$ctrl" = "1" ]; then
      mod="${mod}Ctrl+"
   fi
   if [ "$alt" = "1" ]; then
      mod="${mod}Alt+"
   fi
   if [ "$numlock" = "1" ]; then
      numlockx on
   else
      numlockx off
   fi

   skip=0
   if [[ $key =~ F && $alt = 1 ]]; then
      # Alt+Fx are usually window manager actions
      skip=1
   elif [[ $key =~ Insert && ( $shift = 1 || $ctrl = 1 ) ]]; then
      # {Shift,Ctrl}+Insert is about mouse selection
      skip=1
   elif [[ ( $key =~ Home || $key =~ End || $key =~ Page ) && $shift = 1 ]]; then
      # Shift+{PgUp,PgDn,Home,End} effect the scrollbar
      skip=1
   elif [[ ( $key =~ Up || $key =~ Down ) && $shift = 1 && $ctrl = 1 ]]; then
      # Ctrl+Shift+{Up,Dn} also effect the scrollbar
      skip=1
   elif [[ ( $key =~ Add || $key =~ Sub ) && ( $shift = 1) ]]; then
      # Shift+{+,-} changes window size in xterm
      skip=1
   fi

   echo "$kind $mod$key" >&2
   if [ $skip = 0 ]; then
      xdotool key $mod$key Return
      if [ $? -eq 0 ]; then
         IFS='' read -r esc
         esc="${esc/$'\033'/\\033}"
      else
         echo "SEGV: $mod$key" >&2
         skip=1
      fi
   else
      skip=1
   fi
   if [ $skip = 0 ]; then
      echo "    KH(\"$esc\"), // $kind $mod$key"
   else
      echo "    {NULL, 0}, // $kind $mod$key"
   fi

   sleep 0.1
   # a bit of healthy paranoia
   xdotool key at Return
   IFS='' read -r esc
   if [ "$esc" != '@' ]; then
      echo 'Sync error!' >&2
      exit 1
   fi
}

do_keys_mode() {


   local keys='F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12 Left Right Up Down Home End Insert Delete Prior Next Tab minus space Menu Find Help Select '

   echo "#define KH(in) { in, sizeof(in) - 1 }"

   echo "static const Tty_Key tty_keys[] = {"
   for k in $keys; do
      echo "{"
      echo "  \"$k\","
      echo "  sizeof(\"$k\") - 1,"
      #default
      echo "  {"
      do_reset
      do_key $k 0 0 0 "default"
      do_key $k 0 0 1 "default"
      do_key $k 0 1 0 "default"
      do_key $k 0 1 1 "default"
      do_key $k 1 0 0 "default"
      do_key $k 1 0 1 "default"
      do_key $k 1 1 0 "default"
      do_key $k 1 1 1 "default"
      echo "  },"

      #cursor
      echo "  {"
      do_reset
      echo -ne '\e[?1h' >&2  # application cursor mode
      do_key $k 0 0 0 "cursor"
      do_key $k 0 0 1 "cursor"
      do_key $k 0 1 0 "cursor"
      do_key $k 0 1 1 "cursor"
      do_key $k 1 0 0 "cursor"
      do_key $k 1 0 1 "cursor"
      do_key $k 1 1 0 "cursor"
      do_key $k 1 1 1 "cursor"
      echo "  },"
      echo "},"
   done
   echo "};"


   # Kp_*

   keys='KP_Up KP_Down KP_Right KP_Left KP_Insert KP_Delete KP_Home KP_Prior KP_Next KP_Begin KP_End'

   echo "static const Tty_Key tty_keys_kp_plain[] = {"
   for k in $keys; do
      echo "{"
      echo "  \"$k\","
      echo "  sizeof(\"$k\") - 1,"
      #default
      echo "  {"
      do_reset
      do_key $k 0 0 0 "default"
      do_key $k 0 0 1 "default"
      do_key $k 0 1 0 "default"
      do_key $k 0 1 1 "default"
      do_key $k 1 0 0 "default"
      do_key $k 1 0 1 "default"
      do_key $k 1 1 0 "default"
      do_key $k 1 1 1 "default"
      echo "  },"

      #cursor
      echo "  {"
      do_reset
      echo -ne '\e[?1h' >&2  # application cursor mode
      do_key $k 0 0 0 "cursor"
      do_key $k 0 0 1 "cursor"
      do_key $k 0 1 0 "cursor"
      do_key $k 0 1 1 "cursor"
      do_key $k 1 0 0 "cursor"
      do_key $k 1 0 1 "cursor"
      do_key $k 1 1 0 "cursor"
      do_key $k 1 1 1 "cursor"
      echo "  },"
      echo "},"
   done
   echo "};"
   echo "static const Tty_Key tty_keys_kp_app[] = {"
   for k in $keys; do
      echo "{"
      echo "  \"$k\","
      echo "  sizeof(\"$k\") - 1,"
      #default
      echo "  {"
      do_reset
      echo -ne '\e[?1l' >&2  # normal cursor mode
      echo -ne '\e=' >&2     # application keypad
      do_key $k 0 0 0 "default"
      do_key $k 0 0 1 "default"
      do_key $k 0 1 0 "default"
      do_key $k 0 1 1 "default"
      do_key $k 1 0 0 "default"
      do_key $k 1 0 1 "default"
      do_key $k 1 1 0 "default"
      do_key $k 1 1 1 "default"
      echo "  },"

      #cursor
      echo "  {"
      do_reset
      echo -ne '\e[?1h' >&2  # application cursor mode
      echo -ne '\e=' >&2     # application keypad
      do_key $k 0 0 0 "cursor"
      do_key $k 0 0 1 "cursor"
      do_key $k 0 1 0 "cursor"
      do_key $k 0 1 1 "cursor"
      do_key $k 1 0 0 "cursor"
      do_key $k 1 0 1 "cursor"
      do_key $k 1 1 0 "cursor"
      do_key $k 1 1 1 "cursor"
      echo "  },"
      echo "},"
   done
   echo "};"
   #
   #echo '  normal'
   #echo '###   normal' >&2
   #do_one_cursor_and_keypad_mode

   #echo -ne '\e[?1h' >&2  # application cursor mode
   #echo '  cursor'
   #echo '###   cursor' >&2
   #do_one_cursor_and_keypad_mode

   #echo -ne '\e[?1l' >&2  # normal cursor mode
   #echo -ne '\e=' >&2     # application keypad
   #echo '  keypad'
   #echo '###   keypad' >&2
   #do_one_cursor_and_keypad_mode

   #echo -ne '\e[?1h' >&2  # application cursor mode
   #echo '  cursor+keypad'
   #echo '###   cursor+keypad' >&2
   #do_one_cursor_and_keypad_mode

   echo "#undef KH"
}

cat <<END >&2

Don't do anything!
Don't touch your keyboard!  Not even modifiers or numlock!
Don't change window focus!
Just leave it running!

Make sure to have English keyboard layout.

Press Ctrl+C if you really need to abort.

END

# Sometimes the first run of xdotool produces some garbage, no clue why.
xdotool key T e s t space 1 Return
read x
xdotool key T e s t space 2 Return
read x
if [ "$x" != 'Test 2' ]; then
	echo "Oops, xdotool doesn't seem to work!" >&2
	exit 1
fi

# Let's go!

trap 'do_reset; echo >&2; echo >&2; echo "If your keyboard is stuck, press and release the modifiers one by one" >&2' EXIT

do_keys_mode DEFAULT 0

do_reset

echo >&2
echo "Ready." >&2

trap EXIT
