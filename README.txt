         Havok Command Line Tools 1.0
        =============================

    This packages includes an command line application for
      converting Havok models from Skyrim the native format
      to something that can be manipulated or used as a reference 
      for non-derivative works.
      
    Currently the package only converts from native HKX files built
      with Havok 2010.2.0 SDK to the XML form of HKX or to the 
      Gamebryo KF file format.

   
Prerequisites:
  * For Skyrim, you must unpack the BSA files using an appropriate tool.
    For me I will refer to this location as "%TEMP%\Skyrim - Animations"
  
  * I assume you have the 2010.2 Havok Content Tools and SDK from Intel.
    You will need to provide infomation to get access to the tools but is free.
    http://software.intel.com/sites/havok/en/
    
  * For 3ds max support get the niftools.org exporter for Skyrim
    - http://www.niftools.org
    - http://niftools.sourceforge.net/forum/viewtopic.php?f=20&t=3178
  
Quick Start:
  1. Convert Packed HKX to XML HKX files
    * If you do not specify the output folder it will assume that it should write to 
        the same folder and append "-out" the filename.
    * The XML HKX files can be loaded into Havok tools like the Havok Preview Tool    
    > hkxcmd convert "%TEMP%\Skyrim - Animations"
      
  2. Convert Packed HKX to XML HKX files and place in another folder
    * The following will convert all HKX files in the "%TEMP%\Skyrim - Animations" folder
        and subfolders and place them in the "%TEMP%\Skyrim - Animations - Output" folder. 
    > hkxcmd convert "%TEMP%\Skyrim - Animations" "%TEMP%\Skyrim - Animations - Output"

  3. Preview Animation in Havok Preview Tool
    * Load the Preview Tool: Start | Havok | Havok PcXs Content Tools 2010.2.0 | Havok Preview Tool
    * Convert Skeleton.hkx using step 1 or 2 above
    * File | Open... | Browse for converted skeleton
      - Assuming you used Tutorial 2 that would be the following for the chicken      
         "%TEMP%\Skyrim - Animations - Output\meshes\actors\ambient\chicken\character assets\skeleton.hkx"
      - If you get a "Wrong platform for packfile" error when loading then you selected the original skeleton
        not the converted one.  
        
    * File | Add... | Browse for an animation for that skeleton
      - Assuming you used Tutorial 2 that would be the following for the chicken 
         "%TEMP%\Skyrim - Animations - Output\meshes\actors\ambient\chicken\animation\getup.hkx"

  4. Convert HKX file to Gamebryo KF files
    * The following will just dump KF files into same folder as the HKX file
    > hkxcmd exportkf "%TEMP%\Skyrim - Animations"
    
  5. Convert HKX file to Gamebryo KF files into a different folder
    * The following will convert all HKX files in the "%TEMP%\Skyrim - Animations" folder 
        and subfolders and place them in the "%TEMP%\Skyrim - Animations - Output" folder. 
    > hkxcmd exportkf "%TEMP%\Skyrim - Animations" "%TEMP%\Skyrim - Animations - Output"

  6. Import Skyrim Animation into 3ds Max
    * First import the creature into max using niftools max importer
      - Select Import | "Netimmerse/Gamebryo (*.nif, *.kf)" | Pick the file to import
       * Continuing the chicken theme I used the following
       * "%TEMP%\Skyrim - Meshes\meshes\actors\ambient\chicken\character assets\chicken.nif"      
      - If you have problems with meshes try importing the skeleton explicitly before the mesh     
        * Not all creatures use the name skeleton.nif so you might have to manually adjust path
       
    * Assuming this succeeded now import the animation
      - Select Import | "Netimmerse/Gamebryo (*.nif, *.kf)" | Pick the animation
      - Find an animation you which to import such as the getup animation
        * Assuming you used Tutorial 5 that would be the following for the chicken:
         "%TEMP%\Skyrim - Animations - Output\meshes\actors\ambient\chicken\animation\getup.kf"

    
Troubleshooting:
  - A lot of files show that they failed to load when converting.
    * Ignore any errors about files it cannot convert as not every file 
      can be read with the sdk. This is because the SDK only includes Physics and Animation
      support. Bethesda also uses Havok Behaviors which are not supported by the SDK
      and those file cannot be read currently.

  - I keep getting the following message "Wrong platform for packfile" when using the Preview tool
  
    * You are loading the original animations from Bethesda which are not directly 
      compatible with the preview tool.  Follow the tuturial parts 1 and 2 and 
      export the XML HKX files and use those.
    
  - The animation is corrupt after importing into 3ds Max
    * Try Importing the Skeleton.NIF file explicitly before importing the mesh.
    * Some animations just dont import correctly probably because of bone mapping issues

  - When I try to load the converted HKX file into Havok Preview Tool fails.
    * The Havok Preview Tool breaks with large files like the dragon.hkx files.
    * You might be able to convert the file to a packed file.


Known Issues:

  - I could not figure out how bones are mapped because I'm not sure the information is
    present.  I've just assumed that there is a 1-to-1 skeleton to animation track
    but that is obviously not always a correct assumption

  - Havok supports non-uniform scale in animation however Gamebryo does not
    This means any animation exported to the Gamebryo format may lose 
    information when this is present.
  
  - KF was choosen because the niftools.org 3ds max importer largely works
    and this was the easiest way to do this porting.  It may be worth while
    converting to FBX format instead but that is much more work but
    would presumably have broader support.

Source Code:
  * Source is hosted at github.org
    - git://github.org/figment/hkxcmd.git
    - git://github.org/figment/niflib.git
  
Aknowledgements:
  * Niftools team for niflib and the 3ds Max Importer
  * Havok and Intel for the Havok SDK and perpetual license for 3rd party tools
  