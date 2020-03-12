//////////////////////////////////////
//////////HTML GZ EDITOR//////////////
//////////////////////////////////////

1/ Open ESP32 Control Panel on your browser, then save all source code (View page source) to .html file
2/ Convert .html to .html.gz by gzip tool (for Linux) or this online tool (remember to check option 2):
   https://online-converting.com/archives/convert-to-gzip/
2/ Build filetoarray cpp file (GCC or Visual Studio) (SKIP IT if you can run filetoarray.exe)
3/ Use cmd.exe to run this command: filetoarray.exe index.html.gz > camera_index.h
4/ Open camera_index.h and update len and array data to arduino's camera_index.h
5/ Finish, continue to build your code

//////////////lttung1197//////////////
//////////////18/11/2019//////////////