# Security policy

## Reporting a vulnerability

Configure a private security contact before the first public release and
replace this paragraph with the actual reporting address.

## Untrusted input policy

Mesh parsers and repair implementations must enforce:

- checked integer arithmetic;
- explicit file, vertex, triangle, memory, and recursion limits;
- rejection of NaN and infinite coordinates;
- bounded execution time and cancellation points;
- output to a temporary file followed by atomic replacement;
- preservation of the original model on every failure path.
