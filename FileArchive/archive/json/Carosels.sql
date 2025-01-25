---------------------------------------------------------------
-- TOP INTERESTING (POPULAR ITEMS)
---------------------------------------------------------------
select o.[count], o.AugeoProductID , Brand, Name 
from (
	select count(bh.AugeoProductID) [count], bh.AugeoProductID AugeoProductID
	  from [AUGEODB_RH_CMT].[dbo].[Catalog] c
	  join AUGEODB_CATALOG.dbo.BrowsingHistory bh on bh.CatalogID = c.CatalogID
	  join AUGEODB_CATALOG.dbo.cataloguser cu  on bh.CatalogUserID= cu.CatalogUserID
	  where 
		  c.CatalogIdentifier = 'atd_catalog' and
		  cu.UserDatabaseID = 2 and
		  bh.[CreateDate] >= DATEADD(YEAR, -1, GETDATE())
	 group by bh.AugeoProductID
	 ) o 
join AUGEODB_CATALOG.dbo.AugeoProduct ap on ap.AugeoProductID = o.AugeoProductID
where ap.ProductTypeID != 255
order by [count] desc


---------------------------------------------------------------
-- RECENTLY VIEWED / OTHER ITEMS YOU MIGHT ENJOY
---------------------------------------------------------------
select o.BrowsingHistoryID, o.AugeoProductID , Brand, Name 
from (
	select max(bh.BrowsingHistoryID) BrowsingHistoryID, bh.AugeoProductID AugeoProductID
	  from [AUGEODB_RH_CMT].[dbo].[Catalog] c
	  join AUGEODB_CATALOG.dbo.BrowsingHistory bh on bh.CatalogID = c.CatalogID
	  join AUGEODB_CATALOG.dbo.cataloguser cu  on bh.CatalogUserID= cu.CatalogUserID
	  where 
		  cu.Username = 'SSOUser211atdrewardshubaugeo' and
		  cu.ProgramId = 1967 and
		  c.CatalogIdentifier = 'atd_catalog' and
		  cu.UserDatabaseID = 2 and
		  bh.[CreateDate] >= DATEADD(YEAR, -1, GETDATE())
	 group by bh.AugeoProductID
	 ) o 
join AUGEODB_CATALOG.dbo.AugeoProduct ap on ap.AugeoProductID = o.AugeoProductID
where ap.ProductTypeID != 255
order by BrowsingHistoryID desc

---------------------------------------------------------------
-- POPULAR BRANDS
---------------------------------------------------------------
select c.ProductMixId, sp.AugeoProductID,
       sp.SponsorProductID Id,
       spx.AugeoCategoryId AugeoCategoryId,
       spx.RelTypeId SponsorCarouselTypeId,
       sp.SponsorPartyID SponsorPartyId,
       sp.Title Headline,
       sp.SubTitle SubHeadline,
       sp.ProductLink ProductLink,
       sp.HeroPicLink HeroImageUrl,
       sp.SpotlightPicLink SpotlightImageUrl,
       sp.CategoryPicLink 'CategoryImageUrl',
       spx.Rank Rank,
       spx.AugeoCategoryId AugeoCategoryId
from  [AUGEODB_RH_CMT].[dbo].[Catalog] c 
join  [AUGEODB_RH_CMT].[dbo].SponsorshipProfileXcatalog spc on c.CatalogId = spc.CatalogId
join  [AUGEODB_RH_CMT].[dbo].SponsorshipProfileXsponsorProduct spx on spc.SponsorshipProfileId = spx.SponsorshipProfileId
join  [AUGEODB_RH_CMT].[dbo].SponsorProduct sp on spx.SponsorProductId =  sp.SponsorProductId
Where(c.EndDate is null or c.EndDate >= GETDATE())