Read Dx File (Feature Ids) {#dxreader}
======
## Group (Subgroup) ##
I/O Filters (Input)

## Description ##

~~~~~~~~~~~~~~~
This Filter Reads from a Dx file. This is a file in a format generated using the _OpenDX Source Software_ based on IBM's _Visualization Data Explorer_. 
An example of the format is show below:
object 1 class gridpositions counts 6 5 4
origin 0 0 0
delta  1 0 0
delta  0 1 0
delta  0 0 1

object 2 class gridconnections counts 6 5 4

object 3 class array type int rank 0 items 60 data follows
100 101 102 
103 104 105 
106 107 108 
109 110 111 
112 113 114 
115 116 117 
118 119 120 
121 122 123 
124 125 126 
127 128 129 
130 131 132 
133 134 135 
136 137 138 
139 140 141 
142 143 144 
145 146 147 
148 149 150 
151 152 153 
154 155 156 
157 158 159 

attribute "dep" string "connections"

object "DREAM3D Generated" class feature
component  "positions"    value 1
component  "connections"  value 2
component  "data"         value 3

end
~~~~~~~~~~~~~~~

_See: www.opendx.org for more information._


## Parameters ##

| Name | Type |
|------|------|
| Input File | File Path |
| Resolution | X, Y, Z Resolution of the data |
| Origin     | The location in space coordinates of the (0,0,0) voxel |

## Required DataContainers ##

ImageGeometry is required.

## Required Arrays ##

None

## Created Arrays ##

| Type | Default Name | Description | Comment |
|------|--------------|-------------|---------|
| Cell | GrainIds | Ids (ints) that specify to which **Feature** each **Cell** belongs. | Values should be present from segmentation of experimental data or synthetic generation and cannot be determined by this filter. Not having these values will result in the filter to fail/not execute. |




## License & Copyright ##

Please see the description file distributed with this plugin.

## DREAM3D Mailing Lists ##

If you need more help with a filter, please consider asking your question on the DREAM3D Users mailing list:
https://groups.google.com/forum/?hl=en#!forum/dream3d-users

