#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <Magick++.h>
using namespace Magick;
using namespace std;

// Generate a random number between from and to
size_t RandNum(size_t from, size_t to) { return (rand() % (to-from+1)) + from; }

// Information needed in order to queue a image draw operation
struct DrawOperation
{
  Image *pImage;
  size_t x;
  size_t y;
  DrawOperation(Image *image, size_t startX, size_t startY)
    : pImage(image), x(startX), y(startY) { drawQueue.push_back(this); };
  ~DrawOperation() { delete pImage; }

  static vector<DrawOperation *> drawQueue;
  static void Draw(Image &canvas)
  {
    for(int i = 0; i < drawQueue.size(); i++)
    {
      DrawOperation *pDraw = drawQueue[i];
      canvas.composite(*(pDraw->pImage), pDraw->x, pDraw->y);
    }
  }
};
vector<DrawOperation *> DrawOperation::drawQueue;

// Read a text file into memory
struct textfile
{
private:
  vector<string> lines;

public:
  textfile(string fileName)
  {
    ifstream pFile(fileName);
    string line;
    while(getline(pFile, line))
      lines.push_back(line);
    pFile.close();
  }

  string GetLine(size_t lineNum) const { return lines[lineNum]; }
  void DelLine(size_t lineNum) { lines.erase(lines.begin()+lineNum); }
  size_t NumLines() const { return lines.size(); }
};

//
// Global Variables
//

// Default resolution of output image
size_t screenWidth = 1920, screenHeight = 1080;
// Default background color
string canvasColor = "black";
// Default size of main image as a percentage
unsigned char size = 65;
// Corner to place the first image
unsigned char corner;
// Do not attempt to draw in areas less than minDraw number of pixels
size_t minDraw = 150;
bool tileType;
// File extension
string filename = "collage.png";
// Path to image database
string database = "images.txt";

textfile *pImages;

// Tile images in a width x height rectangle.
// xOrigin and yOrigin represent the location of this rectangle relative to the canvas
// minDraw represents the minimum number of pixels to scale an image down to
// tileType determines whether it tiles horizontally or vertically first
void TileImages(size_t width, size_t height, size_t xOrigin, size_t yOrigin, size_t minDraw, bool tileType)
{
  size_t x = xOrigin, y = yOrigin;
  vector<DrawOperation *> drawQueue;
  size_t imageNum;

  // Tile Horizontally
  if(tileType == 0)
  {
    while(1)
    {
      imageNum = RandNum(0, pImages->NumLines()-1);
      Image *pImage = new Image(pImages->GetLine(imageNum));
      pImages->DelLine(imageNum);
      pImage->resize(Geometry("x" + to_string(height)));
      // If the last image wont fit, tile images in the empty space
      if(((x-xOrigin) + pImage->columns()) > width)
      {
        TileImages(width-(x-xOrigin), height, x, y, minDraw, 1);
        break;
      }

      // Put this draw operation in the queue
      drawQueue.push_back(new DrawOperation(pImage, x, y));

      x  += pImage->columns();
      // If we run out of room to draw, center what we have then stop
      if(((x-xOrigin) + minDraw) > width)
      {
        for(size_t j = 0; j < drawQueue.size(); j++)
          drawQueue[j]->x += (width-(x-xOrigin))/2;
        break;
      }
    }
  }
  // Tile Vertically
  else
  {
    while(1)
    {
      imageNum = RandNum(0, pImages->NumLines()-1);
      Image *pImage = new Image(pImages->GetLine(imageNum));
      pImages->DelLine(imageNum);
      pImage->resize(Geometry(to_string(width)));
      // If the last image wont fit, tile images in the empty space
      if((y-yOrigin + pImage->rows()) > height)
      {
        TileImages(width, height-(y-yOrigin), x, y, minDraw, 0);
        break;
      }

      // Put this draw operation in the queue
      drawQueue.push_back(new DrawOperation(pImage, x, y));

      y += pImage->rows();
      // If we run out of room to draw, center what we have then stop
      if((y-yOrigin + minDraw) > height)
      {
        for(size_t j = 0; j < drawQueue.size(); j++)
          drawQueue[j]->y += (height-(y-yOrigin))/2;
        break;
      }
    }
  }
}

int main(int argc, char *argv[])
{
  srand(time(NULL));

  corner = RandNum(0, 3);
  tileType = RandNum(0, 1);

  // Handle our command line arguments
  for(int i = 1; i < argc; i++)
  {
    if(strcmp(argv[i], "--width") == 0 || strcmp(argv[i], "-w") == 0)
      screenWidth = stoi(argv[++i]);
    else if(strcmp(argv[i], "--height") == 0 || strcmp(argv[i], "-h") == 0)
      screenHeight = stoi(argv[++i]);
    else if(strcmp(argv[i], "--color") == 0 || strcmp(argv[i], "-c") == 0)
      canvasColor = argv[++i];
    else if(strcmp(argv[i], "--minDraw") == 0 || strcmp(argv[i], "-d") == 0)
      minDraw = stoi(argv[++i]);
    else if(strcmp(argv[i], "--file") == 0 || strcmp(argv[i], "-f") == 0)
      filename = argv[++i];
    else if(strcmp(argv[i], "--size") == 0 || strcmp(argv[i], "-s") == 0)
      size = stoi(argv[++i]);
    else if(strcmp(argv[i], "--type") == 0 || strcmp(argv[i], "-t") == 0)
    {
      string tiletype = argv[++i];
      if(tiletype == "vertical" || tiletype == "v")
        tileType = 1;
      else // Horizontal
        tileType = 0;
    }
    else if(strcmp(argv[i], "--corner") == 0 || strcmp(argv[i], "-r") == 0)
      corner = stoi(argv[++i]);
    else if(strcmp(argv[i], "--path") == 0 || strcmp(argv[i], "-p") == 0)
      database = argv[++i];
  }

  pImages = new textfile(database);

  // Set up our blank canvas
  Image canvas(to_string(screenWidth)+"x"+to_string(screenHeight), canvasColor.c_str());

  size_t imageNum = RandNum(0, pImages->NumLines()-1);
  Image mainImage(pImages->GetLine(imageNum));
  pImages->DelLine(imageNum);

  // Downscale the image to the specified size but never upscale
  if((float)screenWidth/(float)mainImage.columns() < (float)screenHeight/(float)mainImage.rows())
  {
    size_t imageWidth = (float)size / 100.f * (float)screenWidth;
    if(mainImage.columns() > imageWidth)
      mainImage.resize(Geometry(to_string(imageWidth)));
  }
  else
  {
    size_t imageHeight = (float)size / 100.f * (float)screenHeight;
    if(mainImage.rows() > imageHeight)
      mainImage.resize(Geometry("x" + to_string(imageHeight)));
  }

  // Randomly pick a corner to draw the first image
  // TODO mainX and mainY should replace corner as CL argument
  size_t mainX = 0, mainY = 0;
  switch(corner)
  {
    // Top Left
    case 0:
      mainX = 0;
      mainY = 0;
      break;
    // Top Right
    case 1:
      mainX = screenWidth - mainImage.columns();
      mainY = 0;
      break;
    // Bottom Right
    case 2:
      mainX = screenWidth - mainImage.columns();
      mainY = screenHeight - mainImage.rows();
      break;
    // Bottom Left
    case 3:
      mainX = 0;
      mainY = screenHeight - mainImage.rows();
      break;
  }

  // Draw the first image
  DrawOperation *pDrawOp = new DrawOperation(&mainImage, mainX, mainY);
  
  // Tile images horizontally first and then vertically
  if(tileType == 0)
  {
    TileImages(screenWidth, screenHeight-mainImage.rows(),
              0, (mainY == 0 ? mainImage.rows() : 0),
              minDraw, 0);
    TileImages(screenWidth-mainImage.columns(), mainImage.rows(),
              (mainX == 0 ? mainImage.columns() : 0), (mainY == 0 ? 0 : screenHeight - mainImage.rows()),
               minDraw, 1);
  }
  // Tile images vertically first and then horizontally
  else
  {
    TileImages(screenWidth-mainImage.columns(), screenHeight, // width, height
              (mainX == 0 ? mainImage.columns() : 0), 0,      // x,y
              minDraw, 1);
    TileImages(mainImage.columns(), screenHeight-mainImage.rows(),
               mainX,  (mainY == 0 ? mainImage.rows() : 0),
               minDraw, 0);
  }

  DrawOperation::Draw(canvas);
  canvas.write(filename);
  return 0;
}
