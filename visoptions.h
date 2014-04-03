#ifndef VISOPTIONS_H
#define VISOPTIONS_H

#include "colormap.h"

class VisOptions
{
public:
    VisOptions(bool _showAgg = true,
                     bool _metricTraditional = true,
                     QString _metric = "Lateness");
    VisOptions(const VisOptions& copy);
    void setRange(double low, double high);

    enum ColorMapType { SEQUENTIAL, DIVERGING, CATEGORICAL };
    enum MessageType { NONE, TRUE, SINGLE };

    bool showAggregateSteps;
    bool colorTraditionalByMetric;
    MessageType showMessages;
    bool topByCentroid;
    bool showInactiveSteps;
    QString metric;
    ColorMapType maptype;
    ColorMap * divergentmap;
    ColorMap * rampmap;
    ColorMap * catcolormap; // categorical colormap
    ColorMap * colormap;
};

#endif // VISOPTIONS_H
