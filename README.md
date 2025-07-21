# Watch-Clip
"Watch Clip" is a tool for Rec Room to freeze the process thread.

(This is NOT a cheat, it does/will not in any way manipulate game memory or data.)

*Usage/about - "Watch Clip" is an old Rec Room quest speedrunning method originally for screen-mode users.
It worked by climbing on an object, and then lagging yourself with your watch GUI to prolong the animation, pushing you through walls, hence the name "Watch Clip/clipping".
This tool is designed to connect to Rec Room and use a custom hotkey to lag your game by freezing it.
Prior to this, Bandicam was typically used because it allowed you to use a hotkey to lag Rec Room, but it wasn't very inefficient because it uses more complicated methods-
of limiting framerates which isn't as steady as using simple Windows API to freeze the game. (Freezing also allows better performance, especially if you want more lag for larger walls more cleanly.)

#Building instructions - Install chocolatey, and run in you powershell/cmd "choco install mingw".
After that, you can edit the source code, replace the icon, or sounds (make sure new audios are compatible with the source via names and types).
Once you adjust anything, you can build it with the pre added compiler command for mingw.

Audios aren't from Rec Room ;)
