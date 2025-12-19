package org.lsposed.manager.ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;
import com.google.android.material.color.MaterialColors;

public class MaterialSwipeRefreshLayout extends SwipeRefreshLayout {

    public MaterialSwipeRefreshLayout(Context context) {
        this(context, null);
    }

    public MaterialSwipeRefreshLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setColorSchemeColors(MaterialColors.getColor(this, com.google.android.material.R.attr.colorPrimary));
        setProgressBackgroundColorSchemeColor(MaterialColors.getColor(this, com.google.android.material.R.attr.colorSurfaceContainer));
    }
}
