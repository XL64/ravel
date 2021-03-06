//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Ravel.
// Written by Kate Isaacs, kisaacs@acm.org, All rights reserved.
// LLNL-CODE-663885
//
// For details, see https://github.com/scalability-llnl/ravel
// Please also see the LICENSE file for our notice and the LGPL.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License (as published by
// the Free Software Foundation) version 2.1 dated February 1999.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
// conditions of the GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//////////////////////////////////////////////////////////////////////////////
#include "commevent.h"
#include <otf2/OTF2_AttributeList.h>
#include <otf2/OTF2_GeneralDefinitions.h>
#include <iostream>
#include "metrics.h"
#include "rpartition.h"

CommEvent::CommEvent(unsigned long long _enter, unsigned long long _exit,
                     int _function, int _entity, int _pe, int _phase)
    : Event(_enter, _exit, _function, _entity, _pe),
      partition(NULL),
      comm_next(NULL),
      comm_prev(NULL),
      true_next(NULL),
      true_prev(NULL),
      pe_next(NULL),
      pe_prev(NULL),
      add_order(0),
      extent_begin(_enter),
      extent_end(_exit),
      atomic(-1),
      matching(-1),
      last_stride(NULL),
      next_stride(NULL),
      last_recvs(NULL),
      last_step(-1),
      stride_parents(new QSet<CommEvent *>()),
      stride_children(new QSet<CommEvent *>()),
      stride(-1),
      step(-1),
      phase(_phase),
      gvid("")
{
}

CommEvent::~CommEvent()
{
    if (last_recvs)
        delete last_recvs;
    if (stride_children)
        delete stride_children;
    if (stride_parents)
        delete stride_parents;
}


bool CommEvent::operator<(const CommEvent &event)
{
    if (enter == event.enter)
    {
        if (add_order == event.add_order)
        {
            if (isReceive())
                return true;
            else
                return false;
        }
        return add_order < event.add_order;
    }
    return enter < event.enter;
}

bool CommEvent::operator>(const CommEvent &event)
{
    if (enter == event.enter)
    {
        if (add_order == event.add_order)
        {
            if (isReceive())
                return false;
            else
                return true;
        }
        return add_order > event.add_order;
    }
    return enter > event.enter;
}

bool CommEvent::operator<=(const CommEvent &event)
{
    if (enter == event.enter)
    {
        if (add_order == event.add_order)
        {
            if (isReceive())
                return true;
            else
                return false;
        }
        return add_order <= event.add_order;
    }
    return enter <= event.enter;
}

bool CommEvent::operator>=(const CommEvent &event)
{
    if (enter == event.enter)
    {
        if (add_order == event.add_order)
        {
            if (isReceive())
                return false;
            else
                return true;
        }
        return add_order >= event.add_order;
    }
    return enter >= event.enter;
}

bool CommEvent::operator==(const CommEvent &event)
{
    return enter == event.enter
            && add_order == event.add_order
            && isReceive() == event.isReceive();
}

bool CommEvent::hasMetric(QString name)
{
    if (metrics->hasMetric(name))
        return true;
    else if (caller && caller->metrics->hasMetric(name))
        return true;
    else
        return partition->metrics->hasMetric(name);
}

double CommEvent::getMetric(QString name, bool aggregate)
{
    if (metrics->hasMetric(name))
        return metrics->getMetric(name, aggregate);

    if (caller && caller->metrics->hasMetric(name))
        return caller->metrics->getMetric(name, aggregate);

    if (partition->metrics->hasMetric(name))
        return partition->metrics->getMetric(name, aggregate);

    return 0;
}

unsigned long long CommEvent::getAggDuration()
{
    // Since we don't know when the trace started recording, we never
    // count the first aggregate duration as it could be misleading
    unsigned long long agg_duration = 0;

    if (comm_prev)
    {
        agg_duration = enter - comm_prev->exit;
    }

    return agg_duration;
}

void CommEvent::calculate_differential_metric(QString metric_name,
                                              QString base_name, bool aggregates)
{
    long long max_parent = metrics->getMetric(base_name, true);
    long long max_agg_parent = 0;
    if (aggregates && comm_prev)
        max_agg_parent = (comm_prev->metrics->getMetric(base_name));

    if (aggregates)
        metrics->addMetric(metric_name,
                           std::max(0.,
                                    getMetric(base_name)- max_parent),
                           std::max(0.,
                                    getMetric(base_name, true)- max_agg_parent));
    else
        metrics->addMetric(metric_name,
                           std::max(0.,
                                    getMetric(base_name)- max_parent));
}

void CommEvent::writeOTF2Leave(OTF2_EvtWriter * writer, QMap<QString, int> * attributeMap)
{
    // For coalesced steps, don't write attributes
    if (step < 0)
    {
        // Finally write enter event
        OTF2_EvtWriter_Leave(writer,
                             NULL,
                             exit,
                             function);
        return;
    }

    OTF2_AttributeList * attribute_list = OTF2_AttributeList_New();

    // Phase and Step
    OTF2_AttributeValue phase_value;
    phase_value.uint32 = phase;
    OTF2_AttributeList_AddAttribute(attribute_list,
                                    attributeMap->value("phase"),
                                    OTF2_TYPE_UINT64,
                                    phase_value);

    OTF2_AttributeValue step_value;
    step_value.uint32 = step;
    OTF2_AttributeList_AddAttribute(attribute_list,
                                    attributeMap->value("step"),
                                    OTF2_TYPE_UINT64,
                                    step_value);

    // Write metrics
    for (QMap<QString, int>::Iterator attr = attributeMap->begin();
         attr != attributeMap->end(); ++attr)
    {
        if (!metrics->hasMetric(attr.key()))
            continue;

        OTF2_AttributeValue attr_value;
        attr_value.uint64 = metrics->getMetric(attr.key());
        OTF2_AttributeList_AddAttribute(attribute_list,
                                        attributeMap->value(attr.key()),
                                        OTF2_TYPE_UINT64,
                                        attr_value);

        OTF2_AttributeValue agg_value;
        agg_value.uint64 = metrics->getMetric(attr.key(), true);
        OTF2_AttributeList_AddAttribute(attribute_list,
                                        attributeMap->value(attr.key() + "_agg"),
                                        OTF2_TYPE_UINT64,
                                        agg_value);
    }

    // Finally write enter event
    OTF2_EvtWriter_Leave(writer,
                         attribute_list,
                         exit,
                         function);
}
