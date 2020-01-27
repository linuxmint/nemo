#! /bin/bash

# Gets the new folder name
DIRNAME=$(zenity --entry --width 600 --title="Create New Folder" --text="Enter the name of the new folder")

# If no name was provided
if [ -z $DIRNAME ]; then
    exit 1
fi

# If the directory already exists
if [ -e $1/$DIRNAME ]; then
    zenity --error --width 350 --title="Error Creating Folder" --text="Directory $DIRNAME already exists"
    exit 1
fi

DIROWNER=$(stat -c %U $1)
if [ $DIROWNER != $USER ]; then
    FILEPATH=$(dirname $BASH_SOURCE)
    export SUDO_ASKPASS="$FILEPATH/askpass.sh"
    sudo -A mkdir "$1/$DIRNAME"
    sudo mv "${@:2}" -t "$1/$DIRNAME" 
    exit 0
else
    mkdir "$1/$DIRNAME"
    mv "${@:2}" -t "$1/$DIRNAME"
    exit 0
fi
