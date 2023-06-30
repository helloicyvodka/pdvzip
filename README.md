# pdvzip
PNG Data Vehicle (ZIP Edition) for Twitter, Imgur, ImgBB, ImgPile, Flickr, ImageShack & PostImage.

Embed a ZIP file within a \*tweetable and "executable" PNG image.  
PNG image size limit is platform dependant:

* Flickr (200MB), Imgbb (32MB), ImageShack (25MB), PostImage (24MB), ImgPile (8MB), *Twitter & Imgur (5MB).

![Demo Image](https://github.com/CleasbyCode/pdvzip/blob/main/demo_images/6_image_vid.png)  
 [***(Image Credit: @Photography_Ai_)***](https://twitter.com/Photography_Ai_/status/1669775346802065423)  

Based on a similar idea from the original Python program ['***tweetable-polyglot-png***'](https://github.com/DavidBuchanan314/tweetable-polyglot-png) created by David Buchanan, pdvzip uses different methods for storing and accessing arbitrary data within a PNG image file.  

Compile and run the program under Windows or **Linux**.

## Usage

```c
$ g++ pdvzip.cpp -s -o pdvzip
$
$ ./pdvzip

Usage:  pdvzip  <png_image>  <zip_file>
        pdvzip  --info

$ ./pdvzip  joker.png  joker_mp4.zip

Created output file 'pdvimg_15061.png' 5232104 bytes.

All Done!

$

```

Once the ZIP file has been embedded within a PNG image, it's ready to be shared on your chosen hosting site or '*executed*' whenever you want to access the embedded file(s).

**Mobile Issue**: Sometimes when saving images from Twitter to a mobile, the file gets saved with a '*.jpg*' extension. Please note, the file has not been converted to a JPG. Twitter has just renamed the extension, so it is still the original PNG image with its embedded content. 

## Extracting Your Embedded File(s)
**Linux**    
Make sure image file has executable permissions.
```c

$ chmod +x pdvimg_15061.png
$
$ ./pdvimg_15061.png

```  
![Screen_shot1](https://github.com/CleasbyCode/pdvzip/blob/main/demo_images/other/screen_shot1.png)

**Windows**   
First, rename the file extension to '*.cmd*'.
```c

G:\demo> ren  pdvimg_15061.png  pdvimg_15061.cmd
G:\demo>
G:\demo> .\pdvimg_15061.cmd

```
 

Opening the cmd file from the desktop, on its first run, Windows may display a security warning. Clear this by clicking '***More info***' then select '***Run anyway***'. To avoid security warnings, run the image file from a Windows command terminal, as shown in the above example.  

For some common video & audio files, Linux requires the '***vlc (VideoLAN)***' application, Windows uses the set default media player.  
PDF '*.pdf*', Linux requires the '***evince***' application, Windows uses the set default PDF viewer.  
[Python](https://asciinema.org/a/544680) '*.py*', Linux & Windows use the '***python3***' command to run these programs.  
PowerShell '*.ps1*', Linux uses the '***pwsh***' command (if PowerShell installed), Windows uses '***powershell***' to run these scripts.

For any other file type, Linux & Windows will rely on the operating system's set default application.  
Obviously, the embedded file needs to be compatible with the operating system you run it on.

If the embedded media type is Python, PowerShell, Shell script or a Windows/Linux executable, you can provide optional command-line arguments for your file.

[**Here is a video example**](https://asciinema.org/a/542549) of using **pdvzip** with a simple Shell script (.sh) with arguments for the script file, that are also embedded within the PNG image along with the script file.
  
To just get access to the file(s) within the ZIP archive, rename the '*.png*' file extension to '*.zip*'. Treat the ZIP archive as read-only, do not add or remove files from the PNG-ZIP polyglot file.




## PNG Image Requirements for Arbitrary Data Preservation


PNG file size (image + embedded content) must not exceed the hosting sites size limits. 
The site will either refuse to upload your image or it will convert your image to ***jpg***, such as Twitter & Imgur.

**Dimensions:**

The following dimension size limits are specific to **pdvzip** and not necessarily the extact hosting site size limits.

**PNG-32** (Truecolour with alpha [6])  
**PNG-24** (Truecolour [2]) 

Image dimensions can be set between a minimum of ***68 x 68*** and a maximum of ***899 x 899***.
    
**PNG-8 (Indexed-colour [3])**

Image dimensions can be set between a minimum of ***68 x 68*** and a maximum of ***4096 x 4096***.
        
**Chunks:**  

PNG chunks that you can insert arbitrary data, in which the hosting site will preserve in conjuction with the above dimensions & file size limits.  

***bKGD, cHRM, gAMA, hIST,***  
***IDAT,*** (Use as last IDAT chunk, after the final image IDAT chunk).  
***PLTE,*** (Use only with PNG-32 & PNG-24 for arbitrary data).  
***pHYs, sBIT, sPLT, sRGB,***  
***tRNS. (Not recommended, may distort image).***
  
This program uses hIST & IDAT chunk names for storing arbitrary data.

## ZIP File Size & Other Important Information

To work out the maximum ZIP file size, start with the hosting site's size limit,
minus your PNG image size, minus 750 bytes (internal shell extraction script size).  
  
Twitter example: (5MB) 5,242,880 - (307,200 + 750) = 4,934,930 bytes available for your ZIP file.  

The less detailed your image, the more space available for the ZIP.

* Make sure your ZIP file is a standard ZIP archive, compatible with Linux unzip & Windows Explorer.
* Do not include other .zip files within the main ZIP archive. (.rar files are ok).
* Do not include other pdvzip created PNG image files within the main ZIP archive, as they are essentially .zip files.
* Use file extensions for your file(s) within the ZIP archive: *my_doc.pdf*, *my_video.mp4*, *my_program.py*, etc.
  
  A file without an extension will be treated as a Linux executable.      
* **Paint.net** application is recommended for easily creating compatible PNG image files.  

Other editions of **pdv** you may find useful:-  

* [jdvrif - JPG Data Vehicle for Reddit](https://github.com/CleasbyCode/jdvrif)
* [pdvrdt - PNG Data Vehicle for Reddit](https://github.com/CleasbyCode/pdvrdt)  
* [pdvps - PNG Data Vehicle for Twitter, PowerShell Edition](https://github.com/CleasbyCode/pdvps)  

##
