# hgt2png

[![Build Status](https://travis-ci.org/garrettsickles/hgt2png.svg?branch=master)](https://travis-ci.org/garrettsickles/hgt2png)

Convert HGT raster to a subset PNG rasters


## Usage
```
Usage: 
        hgt2png <Mode> <HGT Source> <Output Prefix> <HGT Width> <HGT Height> [<Subwidth> <Subheight>]
Note:
        The last two parameters, [<Subwidth> <Subheight>], are optional.
            - If excluded, both default to 1.
            - If included, both must be counting number which evenly subdivide
                  <HGT Width> and <HGT Height>, respectively.

E.G.
        hgt2png a SOURCE.hgt temp/ 3601 3601 2 2

            => temp/SOURCE.0.0.png
            => temp/SOURCE.0.1800.png
            => temp/SOURCE.1800.0.png
            => temp/SOURCE.1800.1800.png

E.G.
        hgt2png r SOURCE.hgt MyData. 3601 3601

            => MyData.SOURCE.0.0.png

```

## Example

### Single Image
```
./hgt2png r N36W113.hgt test/ 3601 3601
```
![Western Grand Canyon](test/N36W113.png)

### Subdivided Images (n x n)
```
./hgt2png r N36W113.hgt test/ 3601 3601 3 3
```

![](test/N36W113.0.0.png) | ![](test/N36W113.0.1200.png) | ![](test/N36W113.0.2400.png)
:----:|:----:|:----:
![](test/N36W113.1200.0.png) | ![](test/N36W113.1200.1200.png) | ![](test/N36W113.1200.2400.png)
![](test/N36W113.2400.0.png) | ![](test/N36W113.2400.1200.png) | ![](test/N36W113.2400.2400.png)

## Building
```
$ make clean
$ make
```


