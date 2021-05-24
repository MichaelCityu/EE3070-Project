function body()
[data,~] = thingSpeakRead(1293952,'Fields',[3,4,7,8],'ReadKey','CQSQB032ST9Y78RV','OutputFormat','table');

if height(data) == 0
    cakeSex = ' ';
    cakeAge = [];
    colaSex = ' ';
    colaAge = [];
else
    cakeSex = data.GenderCake{1};
    cakeAge = str2num(string(data.AgeCake));
    colaSex = data.GenderCola{1};
    colaAge = str2num(string(data.AgeCola));
end

ageBins = [0:10:100,Inf];
labels = ["Male","Female"];
numCake = length(cakeAge);
numCola = length(colaAge);

ratio1 = [nnz(cakeSex == 'M'),nnz(cakeSex == 'F')];
ratio2 = [nnz(colaSex == 'M'),nnz(colaSex == 'F')];
t1 = tiledlayout(2,2,"TileSpacing","compact");

ax1 = nexttile;
histogram(cakeAge,ageBins)
title('cake')

ax2 = nexttile;
histogram(colaAge,ageBins)
title('cola')

xlabel([ax1,ax2],'age')
ylabel([ax1,ax2],'count')
title(ax1,'Age histogram (cake)')
title(ax2,'Age histogram (cola)')

ax3 = nexttile;
pie(ax3,ratio1)
title(ax3,'Gender ratio (cake)')

ax4 = nexttile;
pie(ax4,ratio2)
title(ax4,'Gender ratio (cola)')

lgd = legend(ax4,labels);
lgd.Layout.Tile = 'east';
title(t1,'Customer Analysis',{strcat('Number of cake purchases in the past:',string(numCake)),strcat('Number of cola purchases in the past:',string(numCola))});
end


