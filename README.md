# Minimalist Screen Shot (min-ss)
The purpose of min-ss is to provide a simple and fast way to take a screenshot and save it into
the clipboard on X11. Therefore it does not come with extra features but instead outputs the png directly
to stdout with the purpose of being piped into the desired process.

## Usage
Running min-ss will take a screenshot of the current state of the screen when pressed, and then will
allow you to clip part of the picture by left clicking and dragging the rectangle. Once left click
is release the png made from the selection will be output to stdout.
Alternatively --full can be specified to skip the selection process and output the entire screen as
a png.

## Examples:
Save a selection of the screen screen to the clipboard
```
min-ss | xclip -i -sel clip -t image/png
```
Take a picture of the full screen after 3 seconds, and save it to a file
```
sleep 3 && min-ss --full > output.png
```

