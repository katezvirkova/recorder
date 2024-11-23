 gcc -o vid media.c -I/opt/homebrew/Cellar/ffmpeg/7.1_3/include -L/opt/homebrew/Cellar/ffmpeg/7.1_3/lib -lavcodec -lavformat -lavdevice -lavutil -lswscale
  ./video 10 output.mov "0" 
  
