/***************************************************************
*                                                              *
*                                                              *
*   algorithm: Stores the following information:               *
*              EasyBMPstego (12 bytes)                         *
*              FileNameSize (2 bytes) up to 65536 chars.       *
*              FileName     (FileNameSize bytes)               *
*              FileSize     (4 bytes) up to 4 GB size          *
*              FileData     (FileSize bytes)                   *
*                                                              * 
* Any single byte c is encoded as follows:                     * 
*                                                              *
* c = r1 + 2*g1 + 4*b1 + 8*a1 + 16*r2 + 32*g2 + 64*b2 + 128*a2 *
*     where each of these r1, g1, b1, a1, r2, g2, b2, a2 is    *
*     either 0 or 1.                                           *
*                                                              *
* We then modify two successive pixels by changing their rem-  *
* ainders mod two to equal the (r1,g1,b1,a1) and (r2,g2,b2,a2) *
* calculated above. No red, green, blue, or alpha channel is   *
* changed by more than 1 of the possible [0,255] range, i.e.,  *
* 0.4%.                                                        *
*                                                              *
***************************************************************/

#include "EasyBMP.h"
using namespace std;
#include <cstdio>
#include <cstdlib>

char ExtractChar( BMP& Image, int i, int j)
{
 RGBApixel Pixel1 = *Image(i,j);
 i++; 
 if( i == Image.TellWidth() ){ i=0; j++; }
   
 RGBApixel Pixel2 = *Image(i,j);

 // convert the two pixels to a character
   
 unsigned int T = 0;
 T += (Pixel1.Red%2);
 T += (2* (Pixel1.Green%2));
 T += (4* (Pixel1.Blue%2));
 T += (8* (Pixel1.Alpha%2));
  
 T += (16*  (Pixel2.Red%2));
 T += (32*  (Pixel2.Green%2));
 T += (64*  (Pixel2.Blue%2));
 T += (128* (Pixel2.Alpha%2));
   
 char c = (char) T;
 return c;
}

class EasyBMPstegoInternalHeader
{
 public:
 char* FileName;
 int FileNameSize;
 
 int FileSize;

 unsigned char* CharsToEncode;
 int NumberOfCharsToEncode;
 
 void InitializeFromFile( char* input , int BMPwidth, int BMPheight );
 void InitializeFromImage( BMP& Input );
};

void EasyBMPstegoInternalHeader::InitializeFromImage( BMP& Image )
{ 
 if( Image.TellWidth()*Image.TellHeight() < 2*(12+2+4) )
 {
  CharsToEncode = NULL;
  NumberOfCharsToEncode = 0;
  FileName = NULL;
  FileNameSize = 0;
  FileSize = 0;
  return; 
 }

 // extract the first few purported characters to see if this thing
 // has hidden data

 char* StegoIdentifierString = "EasyBMPstego";
 char ComparisonString [strlen(StegoIdentifierString)+1];
 
 int i=0; 
 int j=0;
 int k=0;
 
 while( k < strlen(StegoIdentifierString) )
 {
  ComparisonString[k] = ExtractChar( Image, i, j);
  i += 2;
  while( i >= Image.TellWidth() )
  { i -= Image.TellWidth(); j++; }
  k++;
 } 
 ComparisonString[k] = '\0';

 if( strcmp( StegoIdentifierString , ComparisonString ) )
 {
  cout << "Error: No (compatible) hidden data found in image!\n";
  FileSize = 0;
  FileNameSize = 0;
  return;
 }
 
 // get the next two characters to determine file size 
 unsigned char C1 = (unsigned char) ExtractChar( Image, i,j );
 i += 2;
 while( i >= Image.TellWidth() )
 { i -= Image.TellWidth(); j++; }
 unsigned char C2 = (unsigned char) ExtractChar( Image, i,j );
 i += 2;
 while( i >= Image.TellWidth() )
 { i -= Image.TellWidth(); j++; }
 
 FileNameSize = C1 + 256*C2;
 FileName = new char[ FileNameSize+1];
 
 // read the filename
 
 k=0;
 while( k < FileNameSize )
 {
  FileName[k] = ExtractChar( Image, i, j);
  i += 2;
  while( i >= Image.TellWidth() )
  { i -= Image.TellWidth(); j++; }
  k++;
 } 
 FileName[k] = '\0';
 
 // find the actual data size
 
 C1 = (unsigned char) ExtractChar( Image, i,j );
 i += 2;
 while( i >= Image.TellWidth() )
 { i -= Image.TellWidth(); j++; }
 C2 = (unsigned char) ExtractChar( Image, i,j );
 i += 2;
 while( i >= Image.TellWidth() )
 { i -= Image.TellWidth(); j++; }
 unsigned char C3 = (unsigned char) ExtractChar( Image, i,j );
 i += 2;
 while( i >= Image.TellWidth() )
 { i -= Image.TellWidth(); j++; }
 unsigned char C4 = (unsigned char) ExtractChar( Image, i,j );
 i += 2;
 while( i >= Image.TellWidth() )
 { i -= Image.TellWidth(); j++; }
  
 FileSize = C1 + 256*C2 + 65536*C3 + 16777216*C4;
 NumberOfCharsToEncode = FileNameSize + 2 + 4 
                       + strlen(StegoIdentifierString);
 return;
}

void EasyBMPstegoInternalHeader::InitializeFromFile( char* input, int BMPwidth, int BMPheight )
{
 FileNameSize = strlen( input ) +1;
 FileName = new char [FileNameSize];
 strcpy( FileName , input );
 FileNameSize--;

 int SPACE = 32; 
 
 char* StegoIdentifierString = "EasyBMPstego";

 NumberOfCharsToEncode = FileNameSize 
                       + strlen(StegoIdentifierString) 
					   + 2 // indicate length of filename
					   + 4; // indicate length of data 
					   
 int MaxCharsToEncode = (int) ( 0.5 * BMPwidth * BMPheight );
 if( NumberOfCharsToEncode >  MaxCharsToEncode )
 {
  cout << "Error: File is too small to even encode file information!\n"
       << "       Terminating encoding.\n";
  FileSize = 0;	 
  CharsToEncode = NULL;
  NumberOfCharsToEncode = 0;
  return;
 } 
					   
 CharsToEncode = new unsigned char [NumberOfCharsToEncode];
 
 FILE* fp;
 fp = fopen( FileName , "rb" );
 if( !fp )
 { 
  FileSize = 0;

  return;
 }
 
 // determine the number of actual data bytes to encode
 
 FileSize = 0;
 while( !feof( fp ) )
 {
  char c;
  fread( &c, 1, 1, fp );
  FileSize++;
 }
 FileSize--;
 
 MaxCharsToEncode -= NumberOfCharsToEncode;
 if( FileSize > MaxCharsToEncode )
 {
  FileSize = MaxCharsToEncode; 
  cout << "Warning: Input file exceeds encoding capacity of the image\n"
       << "         File will be truncated.\n";
 }
 fclose( fp );

 // create this "file header" string
 
 int k = 0;
 
 // this part gives the length of the filename
 while( k < strlen(StegoIdentifierString) )
 { CharsToEncode[k] = StegoIdentifierString[k]; k++; }
 int TempInt = FileNameSize % 256;
 CharsToEncode[k] = (unsigned char) TempInt; k++;
 TempInt = FileNameSize - TempInt;
 if( TempInt < 0 ){ TempInt = 0; }
 TempInt = TempInt / 256; 
 CharsToEncode[k] = (unsigned char) TempInt; k++;

 // this part hides the filename
 int j=0;
 while( j < FileNameSize ) 
 { CharsToEncode[k] = FileName[j]; k++; j++; }
 
 // this part gives the length of the hidden data
 int TempIntOriginal = FileSize;
 TempInt = FileSize % 256;
 CharsToEncode[k] = (unsigned char) TempInt; k++;
 TempIntOriginal -= TempInt;

 if( TempIntOriginal > 0 )
 {
  TempInt = TempIntOriginal % 65536; 
  CharsToEncode[k] = (unsigned char) (TempInt/256); k++;
  TempIntOriginal -= TempInt*256;
 }
 else
 { CharsToEncode[k] = 0; k++; }
 
 if( TempIntOriginal > 0 )
 { 
  TempInt = TempIntOriginal % 16777216; 
  CharsToEncode[k] = (unsigned char) (TempInt/65536); k++;
  TempIntOriginal -= TempInt*65536;
 }
 else
 { CharsToEncode[k] = 0; k++; }
 
 if( TempIntOriginal > 0 )
 {
  TempInt = TempIntOriginal % 4294967296; 
  CharsToEncode[k] = (unsigned char) (TempInt/1677216); k++;
  TempIntOriginal -= TempInt*16777216;
 }
 else
 { CharsToEncode[k] = 0; k++; }
 
 return; 
}

int main( int argc, char* argv[] )
{
 char* DATE = "February 3";
 char* YEAR = "2006";
 char* AUTHOR = "Paul Macklin";
 char* LICENSE = "GPL v. 2";
 char* GROUP = "the EasyBMP Project";
 char* CONTACT = "http://easybmp.sourceforge.net";
 
 bool EncodeMode = false;
 bool DecodeMode = false;
 bool HelpMode = false;
 
 // determine all arguments
 int k=1;
 for( k=1 ; k < argc ; k++ )
 {
  if( *(argv[k]+0) == '-' )
  { 
   if( *(argv[k]+1) == 'h' )
   { HelpMode = true; }
   if( *(argv[k]+1) == 'd' )
   { DecodeMode = true; }
   if( *(argv[k]+1) == 'e' )
   { EncodeMode = true; }   
  }
 }
 
 // rule out sillyness
 
 if( DecodeMode && EncodeMode )
 { HelpMode = true; }
 if( !DecodeMode && !EncodeMode )
 { HelpMode = true; }
 if( EncodeMode && argc < 5 )
 { HelpMode = true; }
 if( DecodeMode && argc < 3 )
 { HelpMode = true; }
 
 // if the user asked for help or is confused, spit out help info
 
 if( argc == 1 || HelpMode )
 {
  cout << "\nEmbedText usage: \n\n"
       << "This embeds a text file into a graphic file, and saves the result to \n"
       << "a new file (-e for encode):\n\n"
       << "          Stego -e <input file> <input bitmap> <output bitmap> \n\n"
       << "This extracts a hidden text message and outputs the result \n"
       << "(-d for decode):\n\n"
       << "          Stego -d <input bitmap>\n\n"
       << "This gives this help information:\n\n"
       << "          Stego -h\n\n"
       << " Created on " << DATE << ", " << YEAR << " by " << AUTHOR << ".\n"
       << " Uses the EasyBMP library, Version " << _EasyBMP_Version_ << ".\n"
       << " Licensed under " << LICENSE << " by " << GROUP << ".\n"
       << " Copyright (c) " << YEAR << " " << GROUP << "\n"
       << " Contact: " << CONTACT << "\n\n";
  return 1;
 }

 // spit out copyright / license information
 cout  << "\n Created on " << DATE << ", " << YEAR << " by " << AUTHOR << ".\n"
       << " Uses the EasyBMP library, Version " << _EasyBMP_Version_ << ".\n"
       << " Licensed under " << LICENSE << " by " << GROUP << ".\n"
       << " Copyright (c) " << YEAR << " " << GROUP << "\n"
       << " Contact: " << CONTACT << "\n\n";
       
	   
 // check to see if it's time to encode or decode

 if( EncodeMode )
 {
  // open the input BMP file
  
  BMP Image;
  Image.ReadFromFile( argv[3] );
  int MaxNumberOfPixels = Image.TellWidth() * Image.TellHeight() - 2;
  int k=1;
  
  // set image to 32 bpp
  
  Image.SetBitDepth( 32 );
  
  // open the input text file
  
  FILE* fp;
  fp = fopen( argv[2] , "rb" );
  if( !fp )
  {
   cout << "Error: unable to read file " << argv[2] << " for text input!" << endl;
   return -1; 
  }

  // figure out what we need to encode as an internal header
  
  EasyBMPstegoInternalHeader IH;
  IH.InitializeFromFile( argv[2] , Image.TellWidth() , Image.TellHeight() );
  if( IH.FileNameSize == 0 || IH.NumberOfCharsToEncode == 0 )
  { return -1; }
  
  // write the internal header;
  k=0;
  int i=0;
  int j=0;
  while( !feof( fp ) && k < IH.NumberOfCharsToEncode )
  {
   // decompose the character 
   
   unsigned int T = (unsigned int) IH.CharsToEncode[k];
   
   int R1 = T % 2;
   T = (T-R1)/2;
   int G1 = T % 2;
   T = (T-G1)/2;
   int B1 = T % 2;
   T = (T-B1)/2;
   int A1 = T % 2;
   T = (T-A1)/2;
   
   int R2 = T % 2;
   T = (T-R2)/2;
   int G2 = T % 2;
   T = (T-G2)/2;
   int B2 = T % 2;
   T = (T-B2)/2;
   int A2 = T % 2;
   T = (T-A2)/2;

   RGBApixel Pixel1 = *Image(i,j);
   Pixel1.Red += ( -Pixel1.Red%2 + R1 );
   Pixel1.Green += ( -Pixel1.Green%2 + G1 );
   Pixel1.Blue += ( -Pixel1.Blue%2 + B1 );
   Pixel1.Alpha += ( -Pixel1.Alpha%2 + A1 );
   *Image(i,j) = Pixel1;
   
   i++; 
   if( i== Image.TellWidth() ){ i=0; j++; }
   
   RGBApixel Pixel2 = *Image(i,j);
   Pixel2.Red += ( -Pixel2.Red%2 + R2 );
   Pixel2.Green += ( -Pixel2.Green%2 + G2 );
   Pixel2.Blue += ( -Pixel2.Blue%2 + B2 );
   Pixel2.Alpha += ( -Pixel2.Alpha%2 + A2 );
   *Image(i,j) = Pixel2;
   
   i++; k++;
   if( i== Image.TellWidth() ){ i=0; j++; }
  }

  // encode the actual data 
  k=0;
  while( !feof( fp ) && k < 2*IH.FileSize )
  {
   char c;
   fread( &c , 1, 1, fp );

   // decompose the character 
   
   unsigned int T = (unsigned int) c;
   
   int R1 = T % 2;
   T = (T-R1)/2;
   int G1 = T % 2;
   T = (T-G1)/2;
   int B1 = T % 2;
   T = (T-B1)/2;
   int A1 = T % 2;
   T = (T-A1)/2;
   
   int R2 = T % 2;
   T = (T-R2)/2;
   int G2 = T % 2;
   T = (T-G2)/2;
   int B2 = T % 2;
   T = (T-B2)/2;
   int A2 = T % 2;
   T = (T-A2)/2;

   RGBApixel Pixel1 = *Image(i,j);
   Pixel1.Red += ( -Pixel1.Red%2 + R1 );
   Pixel1.Green += ( -Pixel1.Green%2 + G1 );
   Pixel1.Blue += ( -Pixel1.Blue%2 + B1 );
   Pixel1.Alpha += ( -Pixel1.Alpha%2 + A1 );
   *Image(i,j) = Pixel1;
   
   i++; k++;
   if( i== Image.TellWidth() ){ i=0; j++; }
   
   RGBApixel Pixel2 = *Image(i,j);
   Pixel2.Red += ( -Pixel2.Red%2 + R2 );
   Pixel2.Green += ( -Pixel2.Green%2 + G2 );
   Pixel2.Blue += ( -Pixel2.Blue%2 + B2 );
   Pixel2.Alpha += ( -Pixel2.Alpha%2 + A2 );
   *Image(i,j) = Pixel2;
   
   
   i++; k++;
   if( i== Image.TellWidth() ){ i=0; j++; }
  }
  
  fclose( fp );
  Image.WriteToFile( argv[4] );
  
  return 0;
 }
 
 if( DecodeMode )
 {
  BMP Image;
 
  Image.ReadFromFile( argv[2] );
  if( Image.TellBitDepth() != 32 )
  {
   cout << "Error: File " << argv[2] << " not encoded with this program." << endl;
   return 1;
  }
  
  EasyBMPstegoInternalHeader IH;
  IH.InitializeFromImage( Image );
  if( IH.FileSize == 0 || IH.FileNameSize == 0 || IH.NumberOfCharsToEncode == 0 )
  {
   cout << "No hiddent data detected. Exiting ... " << endl;
   return -1;
  }

  cout << "Hidden data detected! Outputting to file " << IH.FileName << " ... " << endl;
  
  FILE* fp;
  fp = fopen( IH.FileName , "wb" );
  if( !fp )
  {
   cout << "Error: Unable to open file " << IH.FileName << " for output!\n";
   return -1;
  }
  
  int MaxNumberOfPixels = Image.TellWidth() * Image.TellHeight();
  
  int k=0;
  int i=0;
  int j=0;

  // set the starting pixel to skip the internal header
  i = 2*IH.NumberOfCharsToEncode;
  while( i >= Image.TellWidth() )
  {
   i -= Image.TellWidth(); j++;
  }

  while( k < 2*IH.FileSize )
  {
   // read the two pixels
   
   RGBApixel Pixel1 = *Image(i,j);
   i++; k++;
   if( i == Image.TellWidth() ){ i=0; j++; }
   
   RGBApixel Pixel2 = *Image(i,j);
   i++; k++;
   if( i == Image.TellWidth() ){ i=0; j++; }
   
   // convert the two pixels to a character
   
   unsigned int T = 0;
   T += (Pixel1.Red%2);
   T += (2* (Pixel1.Green%2));
   T += (4* (Pixel1.Blue%2));
   T += (8* (Pixel1.Alpha%2));
   
   T += (16*  (Pixel2.Red%2));
   T += (32*  (Pixel2.Green%2));
   T += (64*  (Pixel2.Blue%2));
   T += (128* (Pixel2.Alpha%2));
   
   char c = (char) T;
   
   fwrite( &c , 1 , 1 , fp ); 
  }
  fclose( fp ); 
  return 0;
 }
 
 return 0;
}
